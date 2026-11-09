"""SHAREABLE SESSION SUMMARY (tools/session_summary.py): a 250 MB report becomes a summary under 20 MB that
keeps every counter section and the key diagnostics counts. Standard library only; runs anywhere."""
import json
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import session_summary as ss  # noqa: E402

DIAG = [
    'diag [  31.328] hybrid: anim->vr (grip-cancel) speed 0.0 rot 0, G->T (grip-cancel) eyes dPos 1.43 m dYaw -34\n',
    'diag [  31.300] hybrid: grip cancel consumed (no action), token 4 fresh 1 web 1 shot 0 tracked 3\n',
    'diag [  31.329] position lock: G->T origin on actor feet (1.0, 2.0, 3.0), anim-view offset discarded 1.40 m\n',
    'diag [  32.330] position lock: residual 0.02 m 1 s into VR\n',
    'diag [  40.000] hybrid: grip-cancel stay ended by native start bits 2\n',
    'diag [  40.001] hybrid: vr->anim (native 8670 ms), T->G\n',
    'diag [  40.001] position lock: residual 0.04 m at VR->anim\n',
    'diag [  50.000] launch facing: momentum 6.1 m/s\n',
]


def big_report(frames=12000, width=400):
    sample = dict(values=list(range(width)), status=3)
    return dict(samples=[sample] * frames, swing_samples=[sample] * frames, motion_samples=[sample] * frames,
                punch=dict(punches=12, issued=12, dropped=0, bots=3), strikes=dict(blows=7),
                punch_samples=[dict(punches=i) for i in range(1500)], shooter=dict(fired=40),
                final=dict(status=4, tracked=9000))


class SessionSummaryTests(unittest.TestCase):
    def test_a_big_report_fits_and_keeps_its_counters(self):
        report = big_report()
        full = len(json.dumps(report, indent=2))
        self.assertGreater(full, 50_000_000)
        text = ss.summarize(report, DIAG)
        self.assertLess(len(text.encode()), ss.LIMIT)
        out = json.loads(text)
        for key in ('punch', 'strikes', 'shooter', 'final'):
            self.assertEqual(out[key], report[key])
        self.assertEqual(len(out['punch_samples']), 1500, 'punch samples cut although small')
        self.assertEqual(len(out['samples']), ss.KEEP)
        self.assertEqual(out['samples'][-1], report['samples'][-1], 'not the newest samples')
        self.assertEqual(out['summary']['cut']['samples'], 12000 - ss.KEEP)

    def test_a_tight_limit_cuts_further_but_never_the_counters(self):
        out = json.loads(ss.summarize(big_report(), DIAG, limit=200_000))
        self.assertLess(out['summary']['kept_per_list'], ss.KEEP)
        self.assertEqual(out['punch']['punches'], 12)
        self.assertEqual(out['summary']['diag']['grip_cancel_consumed'], 1)

    def test_diagnostics_counts(self):
        d = ss.diag_counts(DIAG)
        self.assertEqual((d['grip_cancel_consumed'], d['cancel_stay_ended'], d['anim_to_vr'], d['vr_to_anim'],
                          d['launch_facing']), (1, 1, 1, 1, 1))
        self.assertEqual(d['position_lock_discarded_m']['median'], 1.4)
        self.assertEqual(d['position_lock_residual_m']['1 s into VR']['max'], .02)
        self.assertEqual(d['position_lock_residual_m']['at VR->anim']['max'], .04)
        self.assertEqual(d['switch_eye_jump_m']['grip-cancel']['over_half_metre'], 1)

    def test_written_beside_the_report(self):
        with tempfile.TemporaryDirectory() as folder:
            output = pathlib.Path(folder) / 'game-vr-20261108-221500.json'
            diag = pathlib.Path(folder) / 'game-vr-20261108-221500-diag.log'
            diag.write_text(''.join(DIAG), encoding='utf-8')
            path = ss.write_summary(dict(punch=dict(punches=1), samples=[1, 2, 3]), output, diag)
            self.assertEqual(path.name, 'game-vr-20261108-221500-summary.json')
            out = json.loads(path.read_text(encoding='utf-8'))
            self.assertEqual(out['summary']['diag']['vr_to_anim'], 1)
            self.assertEqual(ss.write_summary({}, output, pathlib.Path(folder) / 'missing.log').name, path.name)

    def test_the_session_writes_it(self):
        source = (ROOT / 'tools' / 'run_game_vr.py').read_text(encoding='utf-8')
        self.assertIn('from session_summary import write_summary', source)
        self.assertIn('write_summary(result, a.output, diag_path(a.output))', source)


if __name__ == '__main__':
    unittest.main()
