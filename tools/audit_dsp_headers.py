#!/usr/bin/env python3
"""Compile DSP, voice, editor and serialized-state interfaces and check boundaries."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPONENT_HEADERS = (
    'HistoryQueue.h', 'Oscillator.h', 'OscillatorSection.h', 'LfoBlock.h',
    'LfoControlProjection.h', 'StereoBalance.h', 'FractionalDial.h', 'ModulationSettings.h', 'ModulationRouter.h', 'OscillatorControlRenderer.h',
    'FilterControlRenderer.h', 'AmplitudeBlockRenderer.h', 'OscillatorAudioRenderer.h',
    'FilterAudioRenderer.h', 'VoiceTuning.h', 'VoiceLfoRenderer.h', 'VoiceLifecycle.h',
    'VoiceCorePolicy.h', 'VoiceEnvelopeConfigurator.h', 'OutputPipeline.h',
    'SharedModulationRuntime.h', 'SharedModulationSettings.h',
    'LfoHistoryEvent.h', 'EffectSettings.h', 'PitchBendSmoother.h', 'OscillatorCoarsePitch.h', 'FmPhaseCorrection.h', 'PulseWidthRead.h',
)
STANDARD_HEADERS = (
    'HistoryQueue.h', 'Oscillator.h', 'OscillatorSection.h', 'LfoBlock.h',
    'LfoControlProjection.h', 'StereoBalance.h', 'FractionalDial.h', 'ModulationSettings.h', 'OscillatorControlRenderer.h', 'FilterControlRenderer.h',
    'AmplitudeBlockRenderer.h', 'VoiceTuning.h', 'VoiceLfoRenderer.h', 'VoiceLifecycle.h', 'VoiceCorePolicy.h',
    'VoiceEnvelopeConfigurator.h', 'SharedModulationSettings.h',
    'LfoHistoryEvent.h', 'EffectSettings.h', 'OutputPipelineSettings.h', 'PitchBendSmoother.h', 'OscillatorCoarsePitch.h', 'FmPhaseCorrection.h', 'PulseWidthRead.h',
)
# Serialized values and numeric I/O must not import the running host/engine.
STATE_FORBIDDEN = ('/ProcessorBridge.h', '/SynthEngine.h', '/SynthVoice.h',
                   '/RenderControls.h', '/dsp/', '/patch/', '/iPlug2/', '/IPlug/', '/IGraphics/')
EDITOR_FORBIDDEN = ('/host/', '/SynthEngine.h', '/SynthVoice.h', '/RenderControls.h',
                    '/iPlug2/', '/IPlug/', '/IGraphics/')
# Each extracted header must compile first, without another header supplying its includes.
ISOLATED_HEADERS = {
    'editor/EditorAppearance.h': EDITOR_FORBIDDEN,
    'editor/EditorTypography.h': EDITOR_FORBIDDEN,
    'editor/EditorFonts.h': EDITOR_FORBIDDEN,
    'editor/EditorInteraction.h': EDITOR_FORBIDDEN,
    'editor/EditorLayout.h': EDITOR_FORBIDDEN,
    'editor/EditorLibraryColumns.h': EDITOR_FORBIDDEN,
    'editor/EditorLibraryNavigation.h': EDITOR_FORBIDDEN,
    'editor/EditorLibraryScroll.h': EDITOR_FORBIDDEN,
    'editor/EditorTextFit.h': EDITOR_FORBIDDEN,
    'editor/EditorPages.h': EDITOR_FORBIDDEN,
    'editor/EditorPresetDisplay.h': EDITOR_FORBIDDEN,
    'editor/EditorValueDisplay.h': EDITOR_FORBIDDEN,
    'editor/ParameterPresentation.h': EDITOR_FORBIDDEN,
    'host/ParameterInfo.h': STATE_FORBIDDEN,
    'host/ProcessorState.h': STATE_FORBIDDEN,
    'serialization/FiniteNumber.h': STATE_FORBIDDEN,
    'midi/ArpeggiatorSettings.h': ('/RenderControls.h', '/Arpeggiator.h', '/SynthEngine.h', '/SynthVoice.h', '/patch/'),
    'midi/Arpeggiator.h': ('/RenderControls.h', '/SynthEngine.h', '/SynthVoice.h', '/patch/'),
    'VoiceCore.h': ('/SynthEngine.h', '/SynthVoice.h', '/VoiceCorePool.h', '/VoiceDispatcher.h', '/RenderControls.h', '/patch/'),
    'VoiceCorePool.h': ('/SynthEngine.h', '/SynthVoice.h', '/VoiceDispatcher.h', '/RenderControls.h', '/patch/'),
    'VoiceDispatcher.h': ('/SynthEngine.h', '/SynthVoice.h', '/VoiceCorePool.h', '/RenderControls.h', '/SharedModulationRuntime.h', '/patch/'),
    'SynthVoice.h': ('/SynthEngine.h', '/VoiceDispatcher.h', '/VoiceCorePool.h'),
    'dsp/Oscillator.h': ('/OscillatorSection.h', '/OscillatorAudioRenderer.h', '/SynthEngine.h', '/SynthVoice.h', '/RenderControls.h', '/patch/'),
    'dsp/OscillatorSection.h': ('/SynthEngine.h', '/SynthVoice.h', '/RenderControls.h', '/patch/'),
    'dsp/LfoBlock.h': ('/Lfo.h', '/LfoClock.h', '/SharedModulationRuntime.h', '/SynthEngine.h', '/SynthVoice.h', '/RenderControls.h', '/patch/'),
}
FORBIDDEN = ('/SynthEngine.h', '/SynthVoice.h', '/RenderControls.h', '/patch/',
             '/juce_', '/iPlug2/', '/IPlug/', '/IGraphics/')


def compile_arguments(build):
    source = ROOT / 'projects/Synth2/src/dsp/SharedModulationRuntime.cpp'
    database = build / 'compile_commands.json'
    if database.is_file():
        for entry in json.loads(database.read_text()):
            directory = Path(entry['directory'])
            if (directory / entry['file']).resolve() == source:
                args = entry.get('arguments') or shlex.split(entry['command'])
                return args, directory, source
        raise RuntimeError('SharedModulationRuntime.cpp is absent from compile_commands.json')
    if not (build / 'build.ninja').is_file():
        raise RuntimeError('Use a Ninja build or configure CMAKE_EXPORT_COMPILE_COMMANDS=ON')
    commands = subprocess.check_output(
        ['ninja', '-C', str(build), '-t', 'commands',
         'CMakeFiles/Synth2Core.dir/projects/Synth2/src/dsp/SharedModulationRuntime.cpp.o'], text=True)
    command = next(line for line in commands.splitlines()
                   if ' -c ' in line and str(source) in line)
    args = shlex.split(command)
    if any(token in args for token in ('&&', ';', '|')):
        raise RuntimeError('Shell-wrapped compiler commands require a compile database with arguments')
    return args, build, source


def header_source(headers):
    return ''.join(f'#include "dsp/{name}"\n' for name in headers) + 'int main() {}\n'


def audit(build, compiler):
    args, directory, original_source = compile_arguments(build)
    clean = []
    i = 0
    while i < len(args):
        arg = args[i]
        if arg in ('-o', '-MT', '-MF', '-MQ'):
            i += 2
            continue
        if arg in ('-c', '-MD', '-MMD', '-MP') or (directory / arg).resolve() == original_source:
            i += 1
            continue
        clean.append(arg)
        i += 1
    component_text, standard_text = header_source(COMPONENT_HEADERS), header_source(STANDARD_HEADERS)
    with tempfile.TemporaryDirectory(prefix='synth2-dsp-header-audit-') as temporary:
        component = Path(temporary) / 'components.cpp'
        component.write_text(component_text)
        command = clean + ['-I' + str(ROOT / 'projects/Synth2/src'), str(component)]
        subprocess.run(command + ['-fsyntax-only'], cwd=directory, check=True)
        dependencies = subprocess.check_output(command + ['-MM'], cwd=directory, text=True).replace('\\', '/')
        for forbidden in FORBIDDEN:
            if forbidden in dependencies:
                raise RuntimeError(f'DSP interfaces depend on forbidden boundary: {forbidden}')
        for index,(header,forbidden_paths) in enumerate(ISOLATED_HEADERS.items()):
            isolated = Path(temporary) / f'isolated-{index}.cpp'
            isolated.write_text(f'#include "{header}"\nint main() {{}}\n')
            isolated_command = clean + ['-I' + str(ROOT / 'projects/Synth2/src'), str(isolated)]
            subprocess.run(isolated_command + ['-fsyntax-only'], cwd=directory, check=True)
            dependencies = subprocess.check_output(isolated_command + ['-MM'], cwd=directory, text=True).replace('\\', '/')
            for forbidden in forbidden_paths:
                if forbidden in dependencies:
                    raise RuntimeError(f'{header} depends on forbidden boundary: {forbidden}')
        standard = Path(temporary) / 'standard.cpp'
        standard.write_text(standard_text)
        subprocess.run([compiler, '-std=c++17', '-I' + str(ROOT / 'projects/Synth2/src'),
                        '-fsyntax-only', str(standard)], check=True)
    return {
        'component_headers': ['projects/Synth2/src/dsp/' + name for name in COMPONENT_HEADERS],
        'standard_library_headers': ['projects/Synth2/src/dsp/' + name for name in STANDARD_HEADERS],
        'no_engine_voice_aggregate_patch_or_apvts_dependencies': True,
        'source_sha256': hashlib.sha256(component_text.encode()).hexdigest(),
        'pure_source_sha256': hashlib.sha256(standard_text.encode()).hexdigest(),
        'isolated_headers': list(ISOLATED_HEADERS),
        'isolated_dependency_boundaries_passed': True,
        'all_passed': True,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    parser.add_argument('--compiler', default='c++', help='Compiler for the standard-library-only check')
    parser.add_argument('--output-json', type=Path)
    args = parser.parse_args()
    try:
        result = audit(args.build_dir.resolve(), args.compiler)
    except (OSError, RuntimeError, StopIteration, subprocess.CalledProcessError) as error:
        parser.exit(1, f'DSP header audit failed: {error}\n')
    if args.output_json:
        args.output_json.write_text(json.dumps(result, indent=2) + '\n')
    print(f'{len(COMPONENT_HEADERS)} component headers compile without Engine/Voice/render aggregate/patch/APVTS; '
          f'{len(STANDARD_HEADERS)} headers compile without JUCE; '
          f'{len(ISOLATED_HEADERS)} headers compile independently within their dependency boundaries')


if __name__ == '__main__':
    main()
