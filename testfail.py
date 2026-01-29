import itertools
import subprocess

def generate_commands(n, k, w):
    commands = []
    for num_failures in range(2, n - k + 1):
        for failure_combination in itertools.combinations(range(n), num_failures):
            command = f"./CodeTest Clay {n} {k} {w} centralize {' '.join(map(str, failure_combination))}"
            commands.append(command)
    return commands

def count_success(commands):
    success_count = 0
    for cmd in commands:
        result = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        output_lines = result.stdout.decode().strip().split('\n')
        last_line = output_lines[-1]
        if last_line == "FAIL!":
            print("Failed command:", cmd)
        else:
            success_count += 1
    return success_count

# 给定的 (n, k, w) 值
configurations = [
    # (4, 2, 4),
    # (6, 4, 8),
    # (9, 6, 27)
    #  (12, 8, 64),
     (14, 10, 256),
    # (16, 12, 256)
]

# 统计每组 (n, k, w) 的成功次数并输出
for n, k, w in configurations:
    commands = generate_commands(n, k, w)
    success_count = count_success(commands)
    print(f"For (n, k, w) = ({n}, {k}, {w}):")
    print(f"SUCCESS count: {success_count}\n")
