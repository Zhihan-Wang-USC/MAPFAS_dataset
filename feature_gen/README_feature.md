# MAPFAS_dataset.feature_gen
 Dataset generation for MAPF Algorithm Selection

**To Compile**
```bash
# make sure you have CMake and boost installed
# brew install boost

cmake -B ./build

cmake --build ./build
```
**To Run**
```bash
cd build
./channelgen --map ../random-32-32-20.map --agents ../random-32-32-20-random-fff.scen --output ../output/ --agentNumMax 20 --agentNumStep 10
```
The output files will be saved to ./output


**To Convert output (.txt) to Pytorch tensor (.pt)**
```bash
# using python3
pip install -r requirements.txt

# the following script will 
# convert output/*.txt to output_tensors/*.pt
python text2tensor.py
```
