@echo off

echo start demo IOC now
echo Type 'dbl' to get a list of all available records.
echo Type 'exit' to terminate this session.
echo ---

"%CATOOLS%"\softIoc.exe -D "%CATOOLS%"\softIoc.dbd -d db\TestPV_ai100000.db

:end
