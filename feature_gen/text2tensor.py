from glob import glob
import torch
import numpy as np
from tqdm import tqdm
import os

# Read text files of 2d array from ./output 
# and convert them to pytorch tensors in ./output_tensors
filenames = glob('./output/*.txt')
assert len(filenames) > 0

if not os.path.exists('./output_tensors'):
    os.mkdir('./output_tensors')

for filename in filenames:
    # Load the 2D array from the text file
    arr = np.loadtxt(filename, skiprows=1)
    # Convert the array to a PyTorch tensor
    tensor = torch.from_numpy(arr)
    # Save the tensor to a file in the "./output_tensors" directory
    # Construct target filename
    target_filename = filename.replace('output', 'output_tensors').replace('.txt', '.pt')
    torch.save(tensor, target_filename)
    print(f"save {filename} to {target_filename}")