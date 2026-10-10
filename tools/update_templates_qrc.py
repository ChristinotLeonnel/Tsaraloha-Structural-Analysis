"""Régénère resources/templates.qrc à partir de resources/templates/builtin (templates intégrés, docs/TEMPLATES.md)."""
import os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "resources"))
lines = ['<!DOCTYPE RCC>', '<RCC version="1.0">',
         '    <!-- Templates intégrés (docs/TEMPLATES.md) : un dossier par paquet (manifest.json + fichiers).',
         '         Liste régénérée par tools/update_templates_qrc.py ; contrôlée par le test 283. -->',
         '    <qresource prefix="/templates">']
for root, dirs, files in os.walk('templates/builtin'):
    dirs.sort()
    for f in sorted(files):
        p = os.path.join(root, f).replace('\\', '/')
        lines.append('        <file alias="%s">%s</file>' % (p[len('templates/'):], p))
lines += ['    </qresource>', '</RCC>', '']
open('templates.qrc', 'w', encoding='utf-8', newline='\n').write('\n'.join(lines))
print(len(lines) - 8, 'fichiers')
