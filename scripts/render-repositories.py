#!/usr/bin/env python3
"""Add the related-repository table from porting.toml to generated device pages."""
from pathlib import Path
import sys
import tomllib
from urllib.parse import quote

def markdown(value):
    return str(value).replace('|', '\\|').replace('[', '\\[').replace(']', '\\]')

def table(record, zh):
    entries = record.get('device', {}).get('repositories', [])
    if not entries:
        return []
    heading = '## 相关仓库 {#repositories}' if zh else '## Related repositories {#repositories}'
    lines = [heading, '', '| 仓库 | 用途 |' if zh else '| Repository | Role |',
             '| --- | --- |']
    for item in entries:
        if not all(isinstance(item.get(key), str) and item[key] for key in ('name', 'url', 'role')):
            raise SystemExit('porting.toml: each repository needs name, url and role')
        lines.append(f'| [{markdown(item["name"])}]({quote(item["url"], safe=":/?&=#%")}) | {markdown(item["role"])} |')
    return lines + ['']

def inject(path, record, zh):
    if not path.is_file():
        return
    text = path.read_text()
    if '{#repositories}' in text:
        return
    marker = '## 配置 {#configuration}' if zh else '## Configuration {#configuration}'
    if marker not in text:
        return
    block = '\n'.join(table(record, zh))
    if not block:
        return
    path.write_text(text.replace(marker, block + '\n' + marker, 1))

def main():
    if len(sys.argv) != 2:
        raise SystemExit(f'usage: {sys.argv[0]} DEVICE')
    device = Path(sys.argv[1])
    record = tomllib.loads((device / 'porting.toml').read_text())
    inject(device / 'docs/devices' / f'{record["device"]["name"]}.md', record, False)
    inject(device / 'docs/zh/devices' / f'{record["device"]["name"]}.md', record, True)

if __name__ == '__main__':
    main()
