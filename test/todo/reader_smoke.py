#!/usr/bin/env python3
"""Generated EPUB, page turn, Home, and Todo after each reader orientation."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

root = Path(__file__).resolve().parents[2]
artifacts = root.parent / 'qa-artifacts'
logs = root.parent / 'logs'
for orientation in range(4):
    with tempfile.TemporaryDirectory(prefix=f'reader-{orientation}-', dir=artifacts) as directory:
        sd = Path(directory)
        (sd/'books').mkdir()
        (sd/'.crosspoint').mkdir()
        with zipfile.ZipFile(sd/'books/alpha.epub', 'w') as book:
            book.writestr('mimetype', 'application/epub+zip', compress_type=zipfile.ZIP_STORED)
            book.writestr('META-INF/container.xml', '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
            book.writestr('OEBPS/content.opf', '<?xml version="1.0"?><package version="2.0" unique-identifier="id" xmlns="http://www.idpf.org/2007/opf"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>ALPHA</dc:title><dc:creator>Offline test</dc:creator><dc:identifier id="id">todo-regression</dc:identifier><dc:language>en</dc:language></metadata><manifest><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/></manifest><spine toc="ncx"><itemref idref="chapter"/></spine></package>')
            book.writestr('OEBPS/toc.ncx', '<?xml version="1.0"?><ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head/><docTitle><text>ALPHA</text></docTitle><navMap><navPoint id="c" playOrder="1"><navLabel><text>ALPHA</text></navLabel><content src="chapter.xhtml"/></navPoint></navMap></ncx>')
            book.writestr('OEBPS/chapter.xhtml', '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>ALPHA</title></head><body><h1>ALPHA</h1>' + '<p>ALPHA reading regression verifies that a chapter still renders and pages turn after adding Todo. Offline books remain available.</p>' * 120 + '</body></html>')
        (sd/'.crosspoint/state.json').write_text(json.dumps({'openEpubPath':'/books/alpha.epub', 'lastSleepFromReader':True}))
        (sd/'.crosspoint/settings.json').write_text(json.dumps({'orientation':orientation}))
        script = ('0:WAIT:Rendered page;400:DOWN;400:WAIT:Rendered page;300:SHOT:' + str(artifacts/f'reader-{orientation}.bmp') +
                  ';500:SWIPE:0.5,0.98,0.5,0.5;500:WAIT:Entering activity: Home;400:SHOT:' + str(artifacts/f'home-after-reader-{orientation}.bmp') + ';700:TAP:180,562;700:WAIT:Entering activity: Todo;300:QUIT')
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', CROSSPOINT_SIM_SD=str(sd), CROSSPOINT_SIM_SCREEN_TEXT='1', CROSSPOINT_SIM_INPUT_SCRIPT=script)
        result = subprocess.run([str(root/'.pio/build/simulator_picco/program')], cwd=root, env=env, capture_output=True, text=True, timeout=45)
        output = result.stdout+result.stderr
        (logs/f'reader-smoke-{orientation}.log').write_text(output)
        assert result.returncode==0, f'orientation {orientation}: check log'
        assert output.count('Rendered page') >= 2 and 'ALPHA' in output
print('EPUB page turn and touch Home passed in all four reader orientations')
