set (ENABLE_MSSQLTEST FALSE CACHE BOOL "Enable MSSQL provider tests")
set (ENABLE_ORACLETEST FALSE CACHE BOOL "Enable Oracle provider tests")
set (ENABLE_HANATEST FALSE CACHE BOOL "Enable HANA provider tests")

include(UsePythonTest)
ADD_PYTHON_TEST(PyQgsProfileSourceRegistry test_qgsprofilesourceregistry.py)


