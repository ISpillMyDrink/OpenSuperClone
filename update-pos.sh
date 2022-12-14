#!/bin/sh

echo "Updating PO template files..."

xgettext -d opensuperclone -s -k_ -kN_ -o ./src/opensuperclone/locale/opensuperclone.pot ./src/opensuperclone/*.c
xgettext -d oscviewer -s -k_ -kN_ -o ./src/oscviewer/locale/oscviewer.pot ./src/oscviewer/*.c

echo "Done updating PO template files."

echo "Updating PO files..."

for i in `find ./src/opensuperclone/locale/ -name opensuperclone.po`; do
    msgmerge -U $i ./src/opensuperclone/locale/opensuperclone.pot --backup=none
done

for i in `find ./src/oscviewer/locale/ -name oscviewer.po`; do
    msgmerge -U $i ./src/oscviewer/locale/oscviewer.pot --backup=none
done

echo "Done updating PO files."

echo "Done."
