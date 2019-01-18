# MyArduinoTools
Some painfully missing tools for Arduino projects

Working on some larger projects beeing a professional developer in C++ i missed some functionality in the Arduino environment. Some tools which are contained are designed by myself, some others are "lent" from STL. However, it is never intended that the implemented tools cover all STL functionality as known in C++14 and up.

## om::FlashFS (FlashFS.h, FlashFS.cpp)
Using a larger EEPROM (\>2kByte) like a micro file system? This comes true with my om::FlashFS implementation. It allows to handle up to 16 (#define, changeable) resurce files of different size on an EEPROM. 
For using the EEPROM as a FlashFS device it needs to be formatted. Thereby a device name is saved along with a small directory structure. The directory holds information about the stored resource files: their name, size and start position. Thus its trivial to check, which EEPROM is plugged into your circuit and if certain resources are already contained.
If the size of your resource changes, its trivial to recreate the file. FlashFS takes care to select a new memory location, selecting the smallest available gap on the chip, large enough to store your data.
Using templates for write() and read() methods allows to handle all 'trivial copyable' data structures directly. 
FlashFS takes care to read data from and write data to the EEPROM effectively. It uses page-writes where ever possible and maintains page boundaries while writing larger chunks of bytes. The buffer size of Wire.h is taken into account, too.

Dependencies: Wire.h, omMemory.h

## om::unique_ptr\<T\> (omMemory.h, header only)
Fighting memory leaks at least with a trivial unique_ptr. Supports everything, that can be deleted using 'free', 'delete' or 'delete[]'. 

Dependencies: stdlib.h

## om::list\<T\> (omList.h, header only)
Manage a dynamic list of data T as known from the big C++ world. Provides iterator, push and pop, splice, find, find_if, remove, remove_if, sort, sort ( Compare ), ... 
T must meet the requirements of CopyAssignable and CopyConstructible. 

Dependencies: none

## more to come...
Libriaries currently tested on Arduino UNO and DUE (!). Any constructive feedback is welcome.
