@echo off

echo start demo IOC now
echo Type 'dbl' to get a list of all available records.
echo Type 'exit' to terminate this session.
echo ---

%APPDATA%\calab\softIoc.exe -D %APPDATA%\calab\softIoc.dbd -d %APPDATA%\calab\Demo\db\demo.db

:end
