
# use python 3.8+ to run this script
from itertools import islice
import subprocess
import os
from datetime import datetime
from pytz import timezone
import pytz
import sys
from glob import glob
import csv
from tqdm import tqdm
import argparse

# %%
dataset_dir = '../datasets/'
output_file = f'local/labelgen.csv'
lns_csv_file = f'local/lns.csv'
file_start = 0
file_end = 1650

map_dir = os.path.join(dataset_dir, "map")
scen_even_dir = os.path.join(dataset_dir, "scen_even")
scen_random_dir = os.path.join(dataset_dir, "scen_random")

lns_exec = '../third_party/MAPF_LNS2/build/lns'
pibt_plus_exec = '../third_party/PIBT2/build/mapf'
runtime = 100


Algos = ["PIBT_PLUS", "CBS", "EECBS", "PP", "PPS"]
# Algos = ["PPS"]
csv_file_prefix = 'lns_run_result'
max_fail_strike = 5

# use glob to get all file under path ../datasets/scen_*/*.scen and sort them
scen_list = sorted(glob(os.path.join(dataset_dir, "scen_*", "*.scen")))

excluded_substr = ["woundedcoast", "orz900d"]

# exclude scen that include the excluded substring
scen_list = [x for x in scen_list if not any(substr in x for substr in excluded_substr)]

# convert a scen file path to map file path
def scen2map(scenfile):
    _, scen_filename = os.path.split(scenfile)
    scen_filename = scen_filename.replace(".scen", "")
    scen_filename = "-".join(scen_filename.split("-")[:-2])
    map_file = os.path.join(map_dir, scen_filename + ".map")
    return map_file

def lns_failed(res: str):
    if (res.find("InitLNS") != -1):
        return True
    if (res.find("Failed to find an initial solution") != -1):
        return True
    return False

def lns_parse_result(stdout):
    r_runtime = re.compile(r"runtime = ([\d\.]+)")
    r_solution_cost = re.compile(r"initial solution cost = (\d+)")
    m = re.search(r_runtime, stdout)
    runtime, solution_cost, solved = -1, -1, -1
    if m:
        runtime = float(m.group(1))
    m = re.search(r_solution_cost, stdout)
    if m:
        solution_cost = int(m.group(1))
    solved = (not lns_failed(stdout)) and runtime > 0 and solution_cost > 0
    if not solved:
        runtime, solution_cost = -1, -1
    return runtime, solution_cost, solved

def run_scen_lns(algo='CBS', scen_file=None, runtime=300, topk=50, csv_file='local/test.csv'):
    map_file = scen2map(scen_file)
    commands = [f'{lns_exec}', '-m', f'{map_file}', '-a', f'{scen_file}',
     '-o', csv_file, '-k', f'{topk}', '-t', f'{runtime}', f'--initAlgo={algo}']
    stdout = subprocess.run(commands, capture_output=True)
    stdout = stdout.stdout.decode("utf-8").replace('\n','')
    comp_time, solution_cost, solved = lns_parse_result(stdout)
    res = {"algo":algo, "scen_file":scen_file, "max_runtime":runtime, "topk":topk, "solved":solved, "soc":solution_cost, "makespan":-1, "comp_time":comp_time, "stdout":stdout}
    return res

import re
r_scen = re.compile(r"\d+\t.+\.map\t\d+\t\d+\t(\d+)\t(\d+)\t(\d+)\t(\d+)\t.+")
MAX_TIMESTEP = 1000
# MAX_TIMESTEP = 2000  # for brc202d.map
ins_file = "local/ins.txt"
result_file = "local/result.txt"
if not os.path.exists(ins_file):
        os.makedirs(os.path.dirname(ins_file), exist_ok=True)
if not os.path.exists(result_file):
        os.makedirs(os.path.dirname(result_file), exist_ok=True)
        with open(result_file, 'w'):
            pass

def pibt_read_result(filename):
    r_solved= re.compile(r"solved=(\d+)")
    r_soc = re.compile(r"soc=(\d+)")
    r_lb_soc = re.compile(r"lb_soc=(\d+)")
    r_makespan = re.compile(r"makespan=(\d+)")
    r_lb_makespan = re.compile(r"lb_makespan=(\d+)")
    r_comp_time = re.compile(r"comp_time=(\d+)")

    solved, soc, lb_soc, makespan, lb_makespan, comp_time = False, -1, -1, -1, -1, -1
    with open(result_file, 'r') as f_read:
        for row in f_read:
            m = re.match(r_solved, row)
            if m:
                solved = int(m.group(1))
                solved = solved==1
            m = re.match(r_soc, row)
            if m:
                soc = int(m.group(1))
            m = re.match(r_lb_soc, row)
            if m:
                lb_soc = int(m.group(1))
            m = re.match(r_makespan, row)
            if m:
                makespan = int(m.group(1))
            m = re.match(r_lb_makespan, row)
            if m:
                lb_makespan = int(m.group(1))
            m = re.match(r_comp_time, row)
            if m:
                comp_time = int(m.group(1))
    return solved, soc, lb_soc, makespan, lb_makespan, comp_time


def run_scen_pibt2(algo, scen_file, num_agents_start, num_agents_end, num_agents_step, runtime, **kwargs):
    assert os.path.exists(pibt_plus_exec)
    map_file = scen2map(scen_file)
    # extract starts and goals
    starts_goals_str = []
    with open(scen_file, 'r') as f:
        for row in f:
            m_scen = re.match(r_scen, row)
            if m_scen:
                y_s = int(m_scen.group(1))
                x_s = int(m_scen.group(2))
                y_g = int(m_scen.group(3))
                x_g = int(m_scen.group(4))
                starts_goals_str.append(f"{y_s},{x_s},{y_g},{x_g}")

    # increments number of agents
    assert num_agents_end <= len(starts_goals_str)
    for num_agents in range(num_agents_start, num_agents_end + 1, num_agents_step):
        if num_agents_start == 0:
            continue
        # create instance
        with open(ins_file, 'w') as f_write:
            f_write.write(f'map_file=../{map_file}\n') # the path is relative 
                                                       # to ins.txt, so we add ../ before
            f_write.write(f'agents={num_agents}\n')
            f_write.write(f'seed=0\n')
            f_write.write(f'random_problem=0\n')
            f_write.write(f'max_timestep={MAX_TIMESTEP}\n')
            f_write.write(f'max_comp_time={runtime}\n')
            config = "\n".join(starts_goals_str[num_agents_start:num_agents])
            f_write.write(f'{config}\n')

        # clear result file
        with open(result_file, 'w'):
            pass

        # solve : algo should be one of SOLVERS = ["PIBT","HCA","PIBT_PLUS","PushAndSwap","PushAndSwap --no-compress"]
        assert algo in ["PIBT","HCA","PIBT_PLUS","PushAndSwap","PushAndSwap --no-compress"]
        stdout = subprocess.run(f"{pibt_plus_exec} -i {ins_file} -o {result_file} -s {algo} -L", capture_output=True, shell=True)
        stdout = stdout.stdout.decode("utf-8").replace('\n','')

        # read result
        solved, soc, lb_soc, makespan, lb_makespan, comp_time = pibt_read_result(result_file)
        res = {"algo":algo, "scen_file":scen_file, "max_runtime":runtime, "topk":num_agents, "solved":solved, "soc":soc, "makespan":makespan, "comp_time":comp_time/1000.0, "stdout":""}
        return res


def run_scen(algo="CBS", **kwargs):
    if algo in ["CBS", "EECBS", "PP", "PPS"]:
        return run_scen_lns(algo, **kwargs)
    elif algo == "PIBT_PLUS":
        topk = kwargs.pop("topk",10)
        return run_scen_pibt2(algo, num_agents_start=topk, num_agents_end=topk, num_agents_step=topk,**kwargs)
    print("algo not supported")

def getDayTime():
    # get timestamp
    day_format = "%Y%m%d"
    time_format = "%H%M%S"
    date = datetime.now(tz=pytz.utc).astimezone(timezone('US/Pacific'))
    day = date.strftime(day_format)
    time = date.strftime(time_format)
    return day, time
    
def run():
    day, time = getDayTime()

    print("starting with ", scen_list[file_start])
    # output_file = f'lns_run_log_{day}_{time}.log'
    # lns_csv_file = f'lns_run_result_{day}_{time}.csv'
    i = 0

    field_names = ["algo", "scen_file", "max_runtime", "topk", "solved", "soc", "makespan", "comp_time", "stdout", "skip"]
    deliminator = ","

    with open(output_file, 'w') as f:
        writer = csv.DictWriter(f, fieldnames=field_names, delimiter=deliminator)
        writer.writeheader()
        for algo in Algos:
            print("Lebeling on ", algo)
            for scen_file in tqdm(scen_list[file_start:file_end]):
                file_agents_count = len(open(scen_file).readlines())-1
                fail_strike = 0
                for k in tqdm(range(10,file_agents_count+1,10),leave=False):
                    if fail_strike >= max_fail_strike:
                        res = {"algo":algo, "scen_file":scen_file, "max_runtime":runtime,
                         "topk":k, "solved":False, "soc":-1, "comp_time":-1, "skip":True}
                    else:
                        res = run_scen(algo=algo, scen_file = scen_file, runtime=runtime, topk=k)
                        if res.get("solved",False) == False:
                            fail_strike += 1
                        elif fail_strike < max_fail_strike:
                            fail_strike = 0
                        # print("fail_strike", fail_strike)
                    writer.writerow(res)
                    f.flush()

import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Process some integers.')
    parser.add_argument('file_start', type=int, default=file_start, help='start index for the input files')
    parser.add_argument('file_end', type=int, default=file_end, help='end index for the input files')
    parser.add_argument('--dataset_dir', default=dataset_dir,help='path to the dataset directory')
    parser.add_argument('--output_file', default=output_file, help='path to the output file')
    args = parser.parse_args()

    file_start, file_end = args.file_start, args.file_end
    dataset_dir, output_file = args.dataset_dir, args.output_file

    print(f"executing files from {file_start} to {file_end}. Results will be saved to {args.output_file}")
    run()
