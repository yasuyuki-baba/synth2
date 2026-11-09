#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>

// Band-limited waveform storage and fixed-point readout shared by the engine
// and component probes. SynthEngine builds the native saw, pulse, triangle and
// sine tables; oscillators select the appropriate frequency band at render time.

namespace synth2
{
    // Synth1-compatible phase accumulator constants.
    // Synth1 AU uses a 27-bit fixed-point phase accumulator and masks with 0x07ffffff.
    constexpr uint32_t kPhaseBits27  = 27u;
    constexpr uint32_t kPhaseScale27 = 1u << kPhaseBits27;
    constexpr uint32_t kPhaseMask27  = 0x07ffffffu;

    struct WaveTableInfo
    {
        // data[] 内の開始オフセット（サンプル単位）
        std::size_t offset = 0;
        // このテーブルの長さ（サンプル数）
        std::size_t length = 0;
        // このテーブルが有効な境界周波数 [Hz]。
        // freq <= freqBorder であればこのテーブル候補になる。
        double freqBorderHz = 0.0;
    };

    struct ShapeTables
    {
        // この shape 用の bandlimited テーブル群
        std::vector<WaveTableInfo> infos;
        // すべてのテーブルを連結したサンプル配列
        std::vector<double> data;
    };

    class WaveTableBank
    {
    public:
        WaveTableBank() = default;

        // shapeIndex に対応するテーブル群をセットする。
        //
        // NOTE: Synth1 では g_tableInfosPointer[shape] / g_waveTablesPointer[shape]
        // に相当するものを初期化していると考えられる。
        void setShapeTables (int shapeIndex, ShapeTables tables)
        {
            if (shapeIndex < 0)
                return;

            if (shapeIndex >= (int) shapes.size())
                shapes.resize ((std::size_t) (shapeIndex + 1));

            shapes[(std::size_t) shapeIndex] = std::move (tables);
        }

        void setAliasBorderHz (double borderHz) { freqAliasBorderHz = std::max (0.0, borderHz); }

        // 指定した shape / freq [Hz] に対応するテーブルの生ポインタと長さを返す。
        // テーブルが存在しない場合は {nullptr, 0} を返す。
        std::pair<const double*, std::size_t> getTableFor (int shapeIndex, double freqHz) const
        {
            if (shapeIndex < 0 || shapeIndex >= (int) shapes.size())
                return { nullptr, 0 };

            const auto& shape = shapes[(std::size_t) shapeIndex];
            const auto n = shape.infos.size();
            if (n == 0 || shape.data.empty())
                return { nullptr, 0 };

            // Synth1 の getWaveTablePointerSize 相当のロジック:
            //   - freqBorder に対して 2 分探索
            //   - 高域かつ idx>0 なら 1 つ前のテーブルを使う

            // 2分探索で freqBorder >= freqHz を満たす最小の idx を探す
            int low = 0;
            int high = (int) n - 1;

            // 境界が単調増加でない場合の保証はしないが、
            // ここでは「だいたい単調」を仮定する。
            while (low < high)
            {
                int mid = low + (high - low) / 2;
                double border = shape.infos[(std::size_t) mid].freqBorderHz;
                if (border < freqHz)
                    low = mid + 1;
                else
                    high = mid;
            }

            int idx = low;
            if (idx < 0)
                idx = 0;
            if (idx > (int) n - 1)
                idx = (int) n - 1;

            // freq が aliasBorder を超えていて、かつ idx>0 なら 1 テーブル戻す
            if (freqHz > freqAliasBorderHz && idx > 0)
                --idx;

            const auto& info = shape.infos[(std::size_t) idx];
            if (info.length == 0 || info.offset + info.length > shape.data.size())
                return { nullptr, 0 };

            return { shape.data.data() + info.offset, info.length };
        }

        // Win64 shape-3 selector (RVA 0x3cad0). Search first strictly greater
        // border, then step back only below the fixed 1575 Hz alias boundary.
        std::pair<const double*, std::size_t> getNativeTableFor (int shapeIndex,double frequency) const
        {
            if (shapeIndex < 0 || static_cast<std::size_t>(shapeIndex) >= shapes.size() || shapes[shapeIndex].infos.empty()) return {nullptr,0};
            const auto& shape = shapes[shapeIndex];
            auto upper = std::upper_bound (shape.infos.begin(),shape.infos.end(),frequency,
                [](double value,const WaveTableInfo& info) { return value < info.freqBorderHz; });
            std::size_t index = upper == shape.infos.end() ? shape.infos.size()-1
                                                         : static_cast<std::size_t> (upper-shape.infos.begin());
            if (frequency != shape.infos[index].freqBorderHz && frequency <= 1575.0 && index > 0) --index;
            const auto& info = shape.infos[index];
            if (info.offset+info.length > shape.data.size()) return {nullptr,0};
            return {shape.data.data()+info.offset,info.length};
        }

        std::pair<const double*, std::size_t> getNativeTriangleFor (double frequency) const
        {
            return getNativeTableFor (2,frequency);
        }

        bool hasShape (int shapeIndex) const
        {
            return shapeIndex >= 0 && shapeIndex < (int) shapes.size() && ! shapes[(std::size_t) shapeIndex].infos.empty();
        }

        std::size_t getNumShapes() const noexcept { return shapes.size(); }

    private:
        std::vector<ShapeTables> shapes;
        double freqAliasBorderHz = 20000.0; // デフォルトは高めに設定しておく
    };

}
