#!/usr/bin/env python3
"""Exercise the serial batch CLI against isolated scenario-output fixtures."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class BatchTests(unittest.TestCase):
    def test_batch(self):
        (ROOT / 'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'build', prefix='all-cells-test-') as directory:
            root = pathlib.Path(directory)
            scripts = root / 'tests/toolbox'
            scripts.mkdir(parents=True)
            registry = root / 'tests/scenarios/scenarios.txt'
            registry.parent.mkdir()
            registry.write_text('a pass\na pixels\na audit\na refused\na extract\na provenance\na unknown\n')
            shutil.copyfile(ROOT / 'tests/toolbox/run-all-cells.sh', scripts / 'run-all-cells.sh')
            runner = scripts / 'run-scenario.sh'
            runner.write_text('''#!/usr/bin/env bash
printf '%s %s %s\\n' "$1" "$2" "${3:-normal}" >>"$(dirname "$0")/calls"
read -r ignored || true
case "$2" in
pass) exit 0 ;;
pixels) echo 'compare result: differing pixels: 42; differing columns: 2; result: fail'; echo 'golden stage failed: settled snapshot differs from fixture' >&2 ;;
audit) echo 'verdict stage failed: audit differs from fixture' >&2 ;;
refused) echo 'machine_verdict=refused' >&2; exit 3 ;;
extract) echo 'extract stage failed: fixture' >&2 ;;
provenance) echo 'provenance stage failed: fixture' >&2 ;;
unknown) exit 7 ;;
esac
exit 1
''')
            runner.chmod(0o755)
            for flag in ('', '--stage-last', '--update-golden'):
                result = subprocess.run(['bash', str(scripts / 'run-all-cells.sh'), *([flag] if flag else [])], text=True, capture_output=True)
                self.assertEqual(result.returncode, 1, result.stderr)
                for cell, outcome in (('pass', 'pass'), ('pixels', 'differs 42'), ('audit', 'audit-differs'), ('refused', 'refused'), ('extract', 'failed extract'), ('provenance', 'refused'), ('unknown', 'failed unknown')):
                    self.assertIn(f'a/{cell} | {outcome} | {root}/build/mame-scenario/a/{cell}', result.stdout)
                self.assertIn('bake: blocked by 5 cell(s): a/audit a/refused a/extract a/provenance a/unknown', result.stdout)
                calls = (scripts / 'calls').read_text().splitlines()
                self.assertEqual(calls, [line + ' ' + (flag or 'normal') for line in registry.read_text().splitlines()])
                (scripts / 'calls').unlink()
            registry.write_text('a pass\na pixels\n')
            result = subprocess.run(['bash', str(scripts / 'run-all-cells.sh')], text=True, capture_output=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn('bake: possible', result.stdout)
            registry.write_text('a pass\n')
            result = subprocess.run(['bash', str(scripts / 'run-all-cells.sh')], text=True, capture_output=True)
            self.assertEqual(result.returncode, 0)
            self.assertIn('bake: possible', result.stdout)
            for flags in (['--probe'], ['--stage-last', '--update-golden']):
                result = subprocess.run(['bash', str(scripts / 'run-all-cells.sh'), *flags], text=True, capture_output=True)
                self.assertEqual(result.returncode, 2)


if __name__ == '__main__':
    unittest.main()
