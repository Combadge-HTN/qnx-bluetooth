"""Read repository metadata only; save downloaded indexes inside this project."""
import concurrent.futures
import io
import pathlib
import re
import subprocess
import tarfile

out = pathlib.Path(__file__).resolve().parent / 'packages' / 'apk-audit-cache'
out.mkdir(parents=True, exist_ok=True)
repos = [s.strip() for s in pathlib.Path('/etc/apk/repositories').read_text().splitlines()
         if s.strip() and not s.lstrip().startswith('#')]
pattern = re.compile(r'audio|sound|alsa|salsa|io-snd|sndrv|\bddk\b|driver development', re.I)

def inspect(item):
    i, repo = item
    url = repo.rstrip('/') + '/aarch64/APKINDEX.tar.gz'
    path = out / ('index-%02d.tar.gz' % i)
    p = subprocess.run(['curl', '--fail', '--location', '--silent', '--show-error',
                        '--max-time', '25', '--output', str(path), url],
                       capture_output=True, text=True)
    if p.returncode:
        return url + '\nFAILED: ' + p.stderr
    with tarfile.open(path) as archive:
        content = archive.extractfile('APKINDEX').read().decode()
    (out / ('index-%02d.txt' % i)).write_text(content)
    matches = []
    for record in content.split('\n\n'):
        fields = dict(line.split(':', 1) for line in record.splitlines() if ':' in line)
        if pattern.search(fields.get('P', '') + ' ' + fields.get('T', '')):
            matches.append(' | '.join(fields.get(k, '') for k in ('P', 'V', 'T')))
    return url + '\n' + '\n'.join(matches) + '\n'

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    results = list(pool.map(inspect, enumerate(repos)))
report = '\n'.join(results)
(out.parent / 'apk-audit-report.txt').write_text(report)
print(report)
