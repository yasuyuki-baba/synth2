"""Naming lint must reject violations while ignoring C++ literal/comment contents."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/lint_naming.py'
SPEC = importlib.util.spec_from_file_location('lint_naming', SCRIPT)
lint = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(lint)


class NamingLint(unittest.TestCase):
    def test_reports_file_type_and_suffix_without_grandfathering(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'patch/core/bad_patch.h'
            path.parent.mkdir(parents=True)
            path.write_text('struct bad_type {};\nstruct DelayParams {};\nclass VoiceConfig {};\n')
            _, errors = lint.audit(root)
            self.assertEqual(len(errors), 4)
            self.assertIn('patch/core/bad_patch.h:2', errors[2])
            self.assertTrue(any('bad_type' in error for error in errors))

    def test_ignores_comments_escaped_literals_and_raw_strings(self):
        source = '''// class bad_comment {};
/* struct bad_block {}; */
const char* value="\\\" class bad_string {};";
auto raw=u8R"x(class bad_raw {}; " \\ )x";
char quote='\\\'';
struct GoodSettings {};
'''
        self.assertEqual(list(lint.declared_types(source)), [('GoodSettings', 6)])

    def test_nested_types_forward_declarations_and_attributes(self):
        source = ('class [[nodiscard]] Good;\nstruct alignas(16) Aligned {};\n'
                  'template<class T> struct Holder { enum class Stage { Idle }; struct State {}; };')
        self.assertEqual([name for name, _ in lint.declared_types(source)],
                         ['Good', 'Aligned', 'T', 'Holder', 'Stage', 'State'])

    def test_scans_multiple_cpp_and_header_types_including_patch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'Patch.h').write_text('struct Patch {}; struct ImportWorkspace {};')
            (root / 'Patch.cpp').write_text('struct ImportSettings {};')
            files, errors = lint.audit(root)
            self.assertEqual(len(files), 2)
            self.assertEqual(errors, [])

    def test_rejects_case_colliding_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'Patch.h').write_text('struct Patch {};')
            (root / 'PATCH.h').write_text('struct Other {};')
            if len(list(root.iterdir())) != 2:
                self.skipTest('Case-insensitive fixture filesystem')
            _, errors = lint.audit(root)
            self.assertTrue(any('path collision' in error for error in errors))

    def test_continued_comments_and_digit_separators_do_not_hide_types(self):
        source = "// struct ignored_name {}; " + "\\" + "\nstruct still_comment {};\n"
        source += "auto first=1'000; struct bad_type {}; auto second=2'000;"
        self.assertEqual(list(lint.declared_types(source)), [('bad_type', 3)])

    def test_alternative_cpp_extensions_cannot_bypass_checks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'bad_header.hpp').write_text('class bad_type {};')
            _, errors = lint.audit(root)
            self.assertEqual(len(errors), 2)

    def test_empty_or_wrong_source_root_cannot_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([sys.executable, str(SCRIPT), '--root', directory],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('No portable C++ sources', result.stderr)

    def test_command_failure_is_actionable_and_success_needs_no_compiler(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'projects/Synth2/src'
            root.mkdir(parents=True)
            path = root / 'Patch.h'
            path.write_text('class patch_name {};')
            result = subprocess.run([sys.executable, str(SCRIPT), '--root', directory],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn('Patch.h:1', result.stderr)
            path.write_text('class Patch {};')
            result = subprocess.run([sys.executable, str(SCRIPT), '--root', directory],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
