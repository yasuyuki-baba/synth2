"""Candidate archive contracts with self-authored bundles and notice files."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SPEC = importlib.util.spec_from_file_location(
    'package_native', Path(__file__).resolve().parents[1] / 'tools/package_native.py')
packager = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packager)
SDK_SPEC = importlib.util.spec_from_file_location(
    'prepare_native_sdks', Path(__file__).resolve().parents[1] / 'tools/prepare_native_sdks.py')
sdk_setup = importlib.util.module_from_spec(SDK_SPEC)
SDK_SPEC.loader.exec_module(sdk_setup)


class CandidatePackages(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.build = self.root / 'build'
        self.output = self.root / 'dist'
        self.manifest = self.root / 'dependencies.json'
        self.manifest.write_text(json.dumps({'product_commit': 'a' * 40,
                                             'iplug2': 'b' * 40}))
        for name in [
            'LICENSE', 'INSTALL.md', 'third_party/miniz/LICENSE', 'third_party/tinyxml2/LICENSE',
            'projects/Synth2/resources/IPLUG2-LICENSE.txt',
            'projects/Synth2/resources/fonts/LICENSE.txt',
            'iPlug2/Dependencies/IPlug/VST3_SDK/LICENSE.txt',
            'iPlug2/Dependencies/IPlug/CLAP_SDK/LICENSE',
            'iPlug2/Dependencies/IPlug/CLAP_HELPERS/LICENSE',
            'iPlug2/WDL/license.txt',
        ]:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('Self-authored placeholder notice\n')

    def binary(self, relative):
        path = self.build / 'out' / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'Self-authored binary placeholder')
        path.chmod(0o755)
        return path

    def test_windows_requires_all_products_and_excludes_unrelated_files(self):
        self.binary('Synth2.vst3/Contents/x86_64-win/Synth2.vst3')
        self.binary('Synth2.clap')
        self.binary('Synth2.exe')
        self.binary('unrelated.pdb')
        result = packager.package(self.root, self.build, 'windows',
                                  self.manifest, self.output)
        with zipfile.ZipFile(result) as archive:
            names = archive.namelist()
            for name in ['Synth2/Synth2.exe', 'Synth2/Synth2.clap',
                         'Synth2/Synth2.vst3/Contents/x86_64-win/Synth2.vst3',
                         'Synth2/notices/iPlug2/WDL/license.txt', 'Synth2/LICENSE',
                         'Synth2/INSTALL.md', 'Synth2/notices/miniz-LICENSE',
                         'Synth2/notices/tinyxml2-LICENSE', 'Synth2/notices/fonts-LICENSE.txt',
                         'Synth2/notices/iPlug2-LICENSE.txt', 'Synth2/notices/VST3-LICENSE.txt',
                         'Synth2/notices/CLAP-LICENSE', 'Synth2/notices/CLAP-helpers-LICENSE']:
                self.assertIn(name, names)
            self.assertNotIn('Synth2/unrelated.pdb', names)
            self.assertEqual(archive.read('Synth2/INSTALL.md'),
                             (self.root / 'INSTALL.md').read_bytes())
            self.assertIn(b'INSTALL.md', archive.read('Synth2/README.txt'))
            self.assertEqual(archive.read('Synth2/dependencies.json'),
                             self.manifest.read_bytes())
        with self.assertRaisesRegex(ValueError, 'replace'):
            packager.package(self.root, self.build, 'windows', self.manifest, self.output)

    @unittest.skipIf(os.name == 'nt', 'Symlink creation requires Windows developer privileges')
    def test_macos_preserves_executable_modes_and_bundle_symlinks(self):
        for suffix in ['vst3', 'clap', 'component', 'app']:
            self.binary(f'Synth2.{suffix}/Contents/MacOS/Synth2')
        (self.build / 'out/Synth2.app/Contents/current').symlink_to('MacOS/Synth2')
        result = packager.package(self.root, self.build, 'macos',
                                  self.manifest, self.output)
        with tarfile.open(result) as archive:
            link = archive.getmember('Synth2/Synth2.app/Contents/current')
            self.assertTrue(link.issym())
            self.assertEqual(link.linkname, 'MacOS/Synth2')
            binary = archive.getmember('Synth2/Synth2.app/Contents/MacOS/Synth2')
            self.assertEqual(binary.mode & 0o111, 0o111)
            self.assertIsNotNone(archive.getmember('Synth2/dependencies.json'))
            self.assertEqual(archive.extractfile('Synth2/INSTALL.md').read(),
                             (self.root / 'INSTALL.md').read_bytes())

    @unittest.skipIf(os.name == 'nt', 'Unix archive modes and symlinks require Unix')
    def test_linux_preserves_products_resources_modes_and_symlinks(self):
        self.binary('Synth2.vst3/Contents/x86_64-linux/Synth2.so')
        self.binary('Synth2.clap')
        self.binary('Synth2')
        resource = self.binary('Synth2.clap.resources/fonts/editor.ttf')
        self.binary('Synth2.resources/img/panel.png')
        (resource.parent / 'current.ttf').symlink_to('editor.ttf')
        self.binary('unrelated.so')
        result = packager.package(self.root, self.build, 'linux',
                                  self.manifest, self.output)
        self.assertEqual(result.name, 'Synth2-linux.tar.gz')
        with tarfile.open(result) as archive:
            for name in ['Synth2/Synth2', 'Synth2/Synth2.clap',
                         'Synth2/Synth2.vst3/Contents/x86_64-linux/Synth2.so']:
                self.assertEqual(archive.getmember(name).mode & 0o111, 0o111)
            link = archive.getmember('Synth2/Synth2.clap.resources/fonts/current.ttf')
            self.assertTrue(link.issym())
            self.assertEqual(link.linkname, 'editor.ttf')
            self.assertEqual(archive.extractfile('Synth2/Synth2.resources/img/panel.png').read(),
                             b'Self-authored binary placeholder')
            self.assertEqual(archive.extractfile('Synth2/dependencies.json').read(),
                             self.manifest.read_bytes())
            self.assertIn('Synth2/INSTALL.md', archive.getnames())
            self.assertIn('Synth2/notices/iPlug2/WDL/license.txt', archive.getnames())
            self.assertNotIn('Synth2/unrelated.so', archive.getnames())

    def test_linux_rejects_a_missing_or_wrong_architecture_bundle(self):
        self.binary('Synth2.clap')
        self.binary('Synth2')
        self.binary('Synth2.vst3/Contents/MacOS/Synth2')
        with self.assertRaisesRegex(ValueError, 'Missing native product'):
            packager.package(self.root, self.build, 'linux', self.manifest, self.output)
        self.assertFalse(self.output.exists())

    def test_partial_bundles_cannot_become_successful_candidates(self):
        self.binary('Synth2.clap')
        self.binary('Synth2.exe')
        self.binary('Synth2.vst3/Contents/Resources/placeholder')
        with self.assertRaisesRegex(ValueError, 'Missing native product'):
            packager.package(self.root, self.build, 'windows', self.manifest, self.output)
        self.assertFalse(self.output.exists())


@unittest.skipUnless(shutil.which('git'), 'Git is required for local SDK fixtures')
class PinnedSDKPreparation(unittest.TestCase):
    def test_fresh_install_preserves_git_clean_crlf_and_rejects_edited_sdk(self):
        # No external checkout, network, user identity or signing key is needed.
        def git(path, *args):
            return subprocess.check_output(['git', '-C', str(path), *args],
                                           text=True, stderr=subprocess.PIPE).strip()

        def commit(path):
            git(path, 'add', '-A')
            git(path, '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                '-c', 'commit.gpgsign=false', 'commit', '-qm', 'Self-authored fixture')

        with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, {
                'GIT_CONFIG_NOSYSTEM': '1', 'GIT_CONFIG_GLOBAL': os.devnull}):
            base = Path(directory)
            source = base / 'format-sdk'
            source.mkdir()
            git(source, 'init', '-q')
            (source / 'control.h').write_bytes(b'// Self-authored SDK fixture\n')
            commit(source)
            sdk = base / 'iplug2'
            sdk.mkdir()
            git(sdk, 'init', '-q')
            placeholder = sdk / 'Dependencies/IPlug/Example/readme.txt'
            placeholder.parent.mkdir(parents=True)
            (sdk / '.gitattributes').write_bytes(b'*.txt text=auto\n')
            placeholder.write_bytes(b'Self-authored scaffold\n')
            commit(sdk)
            git(sdk, 'config', 'core.autocrlf', 'true')
            placeholder.write_bytes(b'Self-authored scaffold\r\n')
            product = base / 'product'
            product.mkdir()
            git(product, 'init', '-q')
            revision = git(source, 'rev-parse', 'HEAD')
            lock = {'iplug2': {'revision': git(sdk, 'rev-parse', 'HEAD')},
                    'native_sdks': {'example': {'repository': source.as_uri(),
                        'revision': revision, 'path': 'Dependencies/IPlug/Example'}}}
            (product / 'dependencies.lock.json').write_text(json.dumps(lock))
            commit(product)
            manifest = base / 'manifest.json'
            result = sdk_setup.prepare(product, sdk / '..' / sdk.name, manifest)
            self.assertEqual(result['example']['revision'], revision)
            self.assertFalse(git(sdk, 'diff', '--name-only', 'HEAD'))
            sdk_setup.prepare(product, sdk, manifest)
            self.assertEqual((placeholder.parent / 'control.h').read_bytes(),
                             b'// Self-authored SDK fixture\n')
            (placeholder.parent / 'control.h').write_bytes(b'// Modified SDK\n')
            with self.assertRaisesRegex(ValueError, 'local changes'):
                sdk_setup.prepare(product, sdk, manifest)


if __name__ == '__main__':
    unittest.main()
