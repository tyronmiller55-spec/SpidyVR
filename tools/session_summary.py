"""SHAREABLE SESSION SUMMARY (the user, November 8 22:59: a session's report runs past 250 MB, too big to send).

Beside every reports/game-vr-<time>.json the session also writes game-vr-<time>-summary.json: the same report
with each long per-frame sample list cut to its newest entries (how many were cut is kept), written without
indentation, plus counts read from the session's diagnostics log (grip cancels, the cancel stay's ends, animation
and SpidyVR switches, position-lock offsets and residuals). Every top-level counter section (punch, strikes,
shooter, slow motion, final sample, ...) is kept whole. The full report is unchanged. Standard library only."""
import json
import pathlib
import re

LIMIT = 20_000_000  # bytes; the summary stays under this
# The per-frame lists that make a report big, and how many of the newest each keeps at first.
BIG = ('samples', 'swing_samples', 'motion_samples', 'grab_samples', 'tie_samples', 'line_samples', 'ray_samples')
SMALL = ('punch_samples', 'strike_samples', 'sense_samples', 'shooter_samples', 'slow_motion_samples')
KEEP = 600

_COUNTS = {
    'grip_cancel_consumed': re.compile(r'hybrid: grip cancel consumed'),
    'cancel_stay_ended': re.compile(r'hybrid: grip-cancel stay ended'),
    'anim_to_vr': re.compile(r'hybrid: anim->vr'),
    'vr_to_anim': re.compile(r'hybrid: vr->anim'),
    'launch_facing': re.compile(r'launch facing: momentum'),
}
_DISCARDED = re.compile(r'position lock: G->T .*discarded (-?[0-9.]+) m')
_RESIDUAL = re.compile(r'position lock: residual (-?[0-9.]+) m (1 s into VR|at VR->anim)')
_EYES = re.compile(r'G->T \(([^)]+)\) eyes dPos (-?[0-9.]+) m')
# Each switch by its reason (the ownership update's anim-end, run-release, land, ...).
_SWITCH = re.compile(r'hybrid: (anim->vr|vr->anim) \(([a-z-]+)')


def _stats(values):
    if not values:
        return dict(count=0)
    ordered = sorted(values)
    return dict(count=len(ordered), median=round(ordered[len(ordered) // 2], 3), max=round(ordered[-1], 3),
                over_half_metre=sum(v > .5 for v in ordered))


def diag_counts(lines):
    """Counts of the key diagnostics lines (an iterable of text lines)."""
    counts = {k: 0 for k in _COUNTS}
    discarded, residual, eyes = [], {'1 s into VR': [], 'at VR->anim': []}, {}
    reasons = {'anim->vr': {}, 'vr->anim': {}}
    for line in lines:
        for key, pattern in _COUNTS.items():
            if pattern.search(line):
                counts[key] += 1
        if m := _DISCARDED.search(line):
            discarded.append(float(m.group(1)))
        if m := _RESIDUAL.search(line):
            residual[m.group(2)].append(float(m.group(1)))
        if m := _EYES.search(line):
            eyes.setdefault(m.group(1), []).append(float(m.group(2)))
        if m := _SWITCH.search(line):
            reasons[m.group(1)][m.group(2)] = reasons[m.group(1)].get(m.group(2), 0) + 1
    return dict(counts, position_lock_discarded_m=_stats(discarded),
                position_lock_residual_m={k: _stats(v) for k, v in residual.items()},
                switch_eye_jump_m={k: _stats(v) for k, v in sorted(eyes.items())},
                switch_reasons={k: dict(sorted(v.items())) for k, v in reasons.items()})


def summarize(report, diag_lines=(), limit=LIMIT):
    """The compact text of `report` (a dict), under `limit` bytes when the cut lists allow it."""
    keep = KEEP
    while True:
        out, cut = {}, {}
        for key, value in report.items():
            if key in BIG + SMALL and isinstance(value, list):
                n = keep if key in BIG else (2000 if keep == KEEP else keep)
                kept = value[-n:] if n else []
                if len(kept) < len(value):
                    cut[key] = len(value) - len(kept)
                out[key] = kept
            else:
                out[key] = value
        out['summary'] = dict(cut=cut, kept_per_list=keep, full_report_unchanged=True,
                              diag=diag_counts(diag_lines))
        text = json.dumps(out, separators=(',', ':'), default=str)
        if len(text.encode('utf-8')) <= limit or keep == 0:
            return text + '\n'
        keep //= 4


def summary_path(output):
    """reports/game-vr-<time>.json -> reports/game-vr-<time>-summary.json"""
    output = pathlib.Path(output)
    return output.with_name(output.stem + '-summary.json')


def write_summary(report, output, diag_path=None, limit=LIMIT):
    """Writes the summary beside `output`; its path, or None when it could not be written."""
    lines = []
    try:
        if diag_path and pathlib.Path(diag_path).exists():
            with open(diag_path, encoding='utf-8', errors='replace') as f:
                lines = f.readlines()
    except OSError:
        lines = []
    path = summary_path(output)
    try:
        path.write_text(summarize(report, lines, limit), encoding='utf-8')
    except (OSError, TypeError, ValueError):
        return None
    return path
