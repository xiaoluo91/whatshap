cd Complete-Striped-Smith-Waterman-Library/src;
make default;
cd ../..;
cd spoa;
mkdir build;
cd build;
cmake -DCMAKE_BUILD_TYPE=Release ..;
make;
cd ../../;
make;
