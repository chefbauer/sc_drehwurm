"""Extrahiert die Overlay-Sektionen aus lvgl_overlay.yaml in separate Dateien."""
import os

INFILE = '/workspaces/display_drehwurm/lvgl_overlay.yaml'
OUTDIR = '/workspaces/display_drehwurm/lvgl_overlays'

with open(INFILE) as f:
    lines = f.readlines()

def extract_overlay(lines, start_id, end_id=None):
    """Findet den - obj: Block mit id: start_id und extrahiert ihn."""
    start = None
    indent = 0
    for i, l in enumerate(lines):
        stripped = l.lstrip()
        if stripped.startswith('- obj:'):
            li = len(l) - len(stripped)
            # Check next few lines for id
            for j in range(i+1, min(i+5, len(lines))):
                if f'id: {start_id}' in lines[j]:
                    start = i
                    indent = li
                    break
        if start is not None:
            break

    if start is None:
        print(f'FEHLER: {start_id} nicht gefunden')
        return None

    # Suche Ende: nächster - obj: auf gleichem Einzug
    end = len(lines)
    for i in range(start + 2, len(lines)):
        l = lines[i]
        stripped = l.lstrip()
        li = len(l) - len(stripped)
        if li <= indent and stripped.startswith('- obj:'):
            if end_id:
                for j in range(i+1, min(i+5, len(lines))):
                    if f'id: {end_id}' in lines[j]:
                        end = i
                        break
            else:
                end = i
            break
        # Auch bei Kommentar-Blöcken auf top-level stoppen wenn end_id gesetzt
        if end_id and li == indent and stripped.startswith('#') and '═══' in stripped:
            end = i
            break

    prefix = indent + 2  # '      - ' = indent + len('- ')
    result = []
    for l in lines[start:end]:
        if l.strip() == '':
            result.append('\n')
        elif len(l) >= prefix:
            result.append(l[prefix:])
        else:
            result.append(l.lstrip())

    # Erste Zeile: '- obj:\n' → 'obj:\n'
    result[0] = result[0].replace('- obj:', 'obj:')
    return result

os.makedirs(OUTDIR, exist_ok=True)

targets = [
    ('overlay_sensorphalanx',    'overlay_temp_messung',     'sensorphalanx.yaml'),
    ('overlay_temp_messung',     'overlay_timer_cowntdown',  'temp_messung.yaml'),
    ('overlay_timer_cowntdown',  None,                       'timer.yaml'),
]

for start_id, end_id, fname in targets:
    content = extract_overlay(lines, start_id, end_id)
    if content:
        path = os.path.join(OUTDIR, fname)
        with open(path, 'w') as f:
            f.writelines(content)
        print(f'OK: {fname} ({len(content)} Zeilen)')
    else:
        print(f'FEHLER: {fname}')
