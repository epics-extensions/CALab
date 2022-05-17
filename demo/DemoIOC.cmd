@echo off

echo start demo IOC now
echo Type 'dbl' to get a list of all available records.
echo Type 'exit' to terminate this session.
echo ---

..\lib\softIoc.exe -D ..\lib\softIoc.dbd -d db\demo.db

:end
