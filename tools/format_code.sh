astyle \
--options=tools/astyle.cfg \
--suffix=none \
--verbose \
--errors-to-stdout \
--exclude=at_host \
--recursive src/*.c,*.h \
$1 $2 $3 # addtional args such as --dry-run etc.