ariel itskovich 214501348
Yovel hazan 206475204

compilation (from src dir)
mkdir obj-intel64
make obj-intel64/ex2.so
$PIN_ROOT/pin -t obj-intel64/ex2.so -- ../bzip2 -k -f ../input.txt

running the given ex2.so from dir root: 
$PIN_ROOT/pin -t ex2.so -- ./bzip2 -k -f ./input.txt