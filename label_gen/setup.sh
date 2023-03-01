# current directory: label_gen
# label_gen
#   setup.sh

dir=$(pwd)

# Download dataset
wget https://movingai.com/benchmarks/mapf/mapf-map.zip
wget https://movingai.com/benchmarks/mapf/mapf-scen-random.zip
wget https://movingai.com/benchmarks/mapf/mapf-scen-even.zip

rm -rf datasets
mkdir datasets
mkdir datasets/map

sudo apt-get update
sudo apt install unzip

unzip mapf-scen-random.zip -d datasets
unzip mapf-scen-even.zip -d datasets
unzip mapf-map.zip -d datasets/map

mv datasets/scen-even datasets/scen_even
mv datasets/scen-random datasets/scen_random

rm mapf*.zip

## For Github Sign-In Only
# cp forCluster/myssh/* ~/.ssh
# sudo chmod 600 ~/.ssh/id_rsa
# sudo chmod 600 ~/.ssh/id_rsa.pub
# eval $(ssh-agent -s)
# ssh-add ~/.ssh/id_rsa

## Install libraries used by LNS2
sudo apt update -y 
sudo apt install libeigen3-dev -y
sudo apt install libboost-all-dev -y

## Install cmake if cmake version is outdated
# mkdir tmp && cd tmp
# wget https://github.com/Kitware/CMake/releases/download/v3.24.3/cmake-3.24.3-linux-x86_64.tar.gz
# tar -xvzf cmake-3.24.3-linux-x86_64.tar.gz
# sudo mv cmake-3.24.3-linux-x86_64 /opt/
# sudo ln -s /opt/cmake-3.24.3-linux-x86_64/bin/cmake /usr/local/bin/

## LNS2
git submodule update --init --recursive
# git submodule add https://github.com/Jiaoyang-Li/MAPF-LNS2.git third_party/MAPF_LNS2
#git clone git@github.com:Jiaoyang-Li/MAPF-LNS2.git MAPF_LNS2
cd $dir
cd third_party/MAPF_LNS2
mkdir build
cmake -B ./build -DCMAKE_BUILD_TYPE=RELEASE
cmake --build ./build --config RELEASE # some warnings are ok
cd build
# test run LNS2
./lns -m ../random-32-32-20.map -a ../random-32-32-20-random-1.scen -o test -k 4 -t 30

## PIBT2
cd $dir
cd third_party/PIBT2
mkdir build
cmake -B ./build -DCMAKE_BUILD_TYPE=RELEASE
cmake --build ./build --config RELEASE # some warnings are ok
cd build
# test run PIBT2
./mapf -i ../instances/mapf/sample.txt -s PIBT -o result.txt -v

### ALL GOOD ENDS ###
