@echo off

echo start demo IOC now
echo Type 'dbl' to get a list of all available records.
echo Type 'exit' to terminate this session.
echo ---

"%CALAB_HOME%\lib\softIoc.exe" -D "%CALAB_HOME%\lib\softIoc.dbd" -d "%CALAB_HOME%\Demo\db\TestPV_ai100000.db"

:end
