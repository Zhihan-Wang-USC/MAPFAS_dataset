## To Generate Labels

```
git clone --recursive https://github.com/Zhihan-Wang-USC/MAPFAS_dataset.git

cd MAPFAS_dataset/label_gen
bash setup.sh

cd scripts
python labelgen.py 234 235 #for illustrating purpose, we use the smallest map which is empty-8-8
python post_labelgen.py 
```

## To Generate Feature (Channels)
```
git clone https://github.com/Zhihan-Wang-USC/MAPFAS_dataset.git

cd MAPFAS_dataset/feature_gen
```
follow [README_feature.md](./feature_gen/README_feature.md)
