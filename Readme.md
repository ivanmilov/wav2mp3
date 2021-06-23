## wav2mp3_converter
tested only on Linux with and without the libmp3lame installed. I do not have an option to test it on Windows, but (probably with small changes/fixes in CMakeLists) it should be compilable :)


### build example:
`mkdir build && cd build`

`cmake .. && make`

`./wav2mp3_converter <path_to_files>`


### notes:
* big-endian architecture is not supported, I do not have an option to test it.
* doubts about thread lib for Win. I suggest using std::thread and supporting compiler will work fine. Not sure about linking though.
* I decided not to create wrapper classes for `wav` and `mp3` because the functionality is very simple and straightforward.
