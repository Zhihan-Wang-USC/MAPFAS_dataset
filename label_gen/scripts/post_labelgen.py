import pandas as pd
import numpy as np

import argparse

# Set up argparse to handle command-line arguments
parser = argparse.ArgumentParser()
parser.add_argument('input', nargs='?', default='local/labelgen.csv', help='input filename (default: data.csv)')
parser.add_argument('output', nargs='?', default='local/labels.csv', help='output filename (default: results.csv)')
args = parser.parse_args()

df = pd.read_csv(args.input)

runtime_penalty_for_unsolved = 5
cost_penalty_for_unsolved = 5

# rename topk to num_agents
# rename comp_time to runtime
# rename scen_file to scenfile
# rename soc to cost
df = df.rename(columns={'topk': 'num_agents', 'comp_time': 'runtime', 'scen_file': 'scenfile', 'soc': 'cost'})
df['algo'] = df['algo'].replace('PIBT_PLUS', 'PIBTPLUS')
df.head()

# drop rows where the solved = False
df = df[df['solved'] != False]

# group the data by matchid and player, and calculate the max and mean scores
grouped = df.groupby(['scenfile', 'num_agents'])\
    .agg({'cost': ['max', 'min'], 'runtime': ['max', 'min']}).reset_index()
grouped.columns = ['_'.join(col[::-1]).strip('_') for col in grouped.columns.values]

pivoted_table = df.pivot_table(index=['scenfile', 'num_agents'], columns='algo', values=['cost','runtime']).reset_index()
pivoted_table.columns = ['_'.join(col[::-1]).strip('_') for col in pivoted_table.columns.values]

result = pd.merge(grouped, pivoted_table, on=['scenfile', 'num_agents'])

solver_names = [col.replace('_cost', '') for col in result.columns if col.endswith('_cost')]
solver_names.remove('min') # b/c there is min_cost

def get_opt_cost_solvers(row):
    solvers = []
    for solver in solver_names:
        if row[solver+'_cost'] == row['min_cost']:
            solvers.append(solver)
    return ' '.join(solvers)

def get_fastest_solver(row):
    solvers = []
    tmp = []
    tmp.append(('min', row['min_runtime']))
    for solver in solver_names:
        tmp.append((solver, row[solver+'_runtime']))
        if row[solver+'_runtime'] == row['min_runtime']:
            # runtime is very unlikely to be the same
            # but if it is, we'll just take the one 
            # with lowest cost
            solvers.append((solver, row[solver+'_cost']))
    solvers.sort()
    if len(solvers) == 0:
        print(tmp)
        return None
    return solvers[0][0]

def replace_nan_with_max_runtime(row):
    for solver in solver_names:
        col_name = solver + '_runtime'
        if pd.isna(row[col_name]):
            row[col_name] = runtime_penalty_for_unsolved * row['max_runtime']
    return row

def replace_nan_with_max_cost(row):
    for solver in solver_names:
        col_name = solver + '_cost'
        if pd.isna(row[col_name]):
            row[col_name] = cost_penalty_for_unsolved * row['max_cost']
    return row

# print(result.head())
result['fastest_solver'] = result.apply(get_fastest_solver, axis=1)
result['opt_cost_solvers'] = result.apply(get_opt_cost_solvers, axis=1)
result = result.apply(replace_nan_with_max_runtime, axis = 1)
result = result.apply(replace_nan_with_max_cost, axis = 1)
result.head()

# save to csv
result.to_csv( args.output, index = False)
print("result saved to " + args.output)