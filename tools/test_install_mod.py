"""Synthetic installer safety checks; never starts a game or touches profiles."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

import install_mod as installer


class InstallationSafety(unittest.TestCase):
    def setUp(self):
        self.parent = Path(tempfile.gettempdir()).resolve()
        self.root = Path(tempfile.mkdtemp(prefix='aitd-install-contract-', dir=self.parent))
        self.game = self.root / 'game'
        self.game.mkdir()
        self.original = b'OWN SYNTHETIC ORIGINAL FIXTURE - NOT GAME CONTENT'
        (self.game / 'Alone.exe').write_bytes(self.original)
        self.sha = hashlib.sha256(self.original).hexdigest()
        self.patch = patch.object(installer, 'STOCK_EXE_SHA256', self.sha)
        self.patch.start()
        self.payload = self.root / 'payload'
        self.payload.mkdir()
        self.first = self.payload / 'renderer'
        self.second = self.payload / 'input'
        self.first.write_bytes(b'own renderer fixture')
        self.second.write_bytes(b'own input fixture')

    def tearDown(self):
        self.patch.stop()
        checked = installer.plain_path(self.root).resolve()
        if checked.parent != self.parent or not checked.name.startswith('aitd-install-contract-'):
            raise AssertionError('Unexpected cleanup destination')
        shutil.rmtree(checked)

    def plan(self):
        return {'d3d9.dll': self.first, 'dinput8.dll': self.second}

    def test_original_identity_rejected(self):
        (self.game / 'Alone.exe').write_bytes(b'different synthetic file')
        with self.assertRaises(ValueError):
            installer.verify_game(self.game)

    def test_missing_shipped_shader_rejected_before_installation(self):
        with self.assertRaisesRegex(ValueError, 'ShaderCache/Offline.sha'):
            installer.verify_game(self.game)

    def test_clean_commit_preserves_original(self):
        with patch.object(installer, 'verify_game', side_effect=lambda game: game):
            result = installer.commit(self.game, self.plan())
        self.assertEqual((self.game / 'Alone.exe').read_bytes(), self.original)
        self.assertEqual((self.game / 'd3d9.dll').read_bytes(), self.first.read_bytes())
        self.assertEqual(set(result['files']), {'d3d9.dll', 'dinput8.dll'})
        self.assertEqual(json.loads((self.game / '.enhanced-install.json').read_text())['original_exe_sha256'], self.sha)

    def test_existing_file_never_overwritten(self):
        existing = self.game / 'dinput8.dll'
        existing.write_bytes(b'user mod')
        with patch.object(installer, 'verify_game', side_effect=lambda game: game):
            with self.assertRaises(ValueError):
                installer.commit(self.game, self.plan())
        self.assertEqual(existing.read_bytes(), b'user mod')
        self.assertFalse((self.game / 'd3d9.dll').exists())

    def test_copy_failure_rolls_back_only_created_files(self):
        sentinel = self.game / 'keep-user-file.txt'
        sentinel.write_bytes(b'keep')
        copy = shutil.copyfileobj
        count = 0

        def failing(input_stream, output):
            nonlocal count
            count += 1
            if count == 2:
                output.write(b'partial')
                raise OSError('Injected disk failure')
            return copy(input_stream, output)

        with patch.object(installer, 'verify_game', side_effect=lambda game: game), patch.object(installer.shutil, 'copyfileobj', side_effect=failing):
            with self.assertRaises(OSError):
                installer.commit(self.game, self.plan())
        self.assertEqual(sentinel.read_bytes(), b'keep')
        self.assertFalse((self.game / 'd3d9.dll').exists())
        self.assertFalse((self.game / 'dinput8.dll').exists())
        self.assertEqual((self.game / 'Alone.exe').read_bytes(), self.original)

    def test_case_collision_rejected_before_writes(self):
        with patch.object(installer, 'verify_game', side_effect=lambda game: game):
            with self.assertRaises(ValueError):
                installer.commit(self.game, {'assets/ui/A.png': self.first, 'assets/ui/a.png': self.second})
        self.assertFalse((self.game / 'assets').exists())

    def test_game_paths_and_devices_rejected(self):
        for name in ('Alone.exe', 'assets/../Alone.exe', '.enhanced/paul.dll',
                     'mods/original.big', 'assets/CON.png', 'assets/ui/a\x00.png',
                     'assets//item.png', 'C:/outside', 'assets/ui./item.png'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                installer.relative_name(name)

    def test_asset_collection_uses_complete_target_prefix(self):
        assets = self.root / 'generated'
        (assets / 'ui').mkdir(parents=True)
        (assets / 'ui' / 'mouse.png').write_bytes(b'synthetic png fixture')
        self.assertEqual(set(installer.collect(assets, 'assets')), {'assets/ui/mouse.png'})


if __name__ == '__main__':
    unittest.main()
