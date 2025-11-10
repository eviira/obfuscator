tmp_dir := .\temp
out_dir := .\output

payload_tmp := ${tmp_dir}\payload.obj
cryptor_tmp := ${tmp_dir}\cryptor.obj
entry_tmp := ${tmp_dir}\entry.obj
injector_tmp := ${tmp_dir}\injector.obj

default_flags := /DYNAMICBASE:NO /Oy- /MT

all: main injector
	${out_dir}\injector.exe

main: compile-entry compile-cryptor compile-payload
	cl.exe -Fe:"${out_dir}\main.exe" ${default_flags} /FIXED /NXCOMPAT:NO ${entry_tmp} ${cryptor_tmp} ${payload_tmp} 

injector: compile-injector compile-cryptor
	cl.exe -Fe:"${out_dir}\injector.exe" ${injector_tmp} ${cryptor_tmp}

compile-entry: .\entry.cpp
	cl.exe -Fo:"${entry_tmp}" -Od /GS- /EHsc ${default_flags} -c $^

compile-cryptor: .\cryptor\cryptor.cpp
	cl.exe -Fo:"${cryptor_tmp}" ${default_flags} -c $^

compile-payload: .\payload\entry.cpp
	cl.exe -Fo:"${payload_tmp}" -Od ${default_flags} -c $^

compile-injector: .\injector.cpp
	cl.exe -Fo:"${tmp_dir}\injector.obj" /EHsc -c $^

test: .\test.cpp
	cl.exe -Fo:"${tmp_dir}\test.obj" -Fe:"${out_dir}\test.exe" /EHsc $^

clean:
	del -Force .\temp\*
	del -Force .\output\*
