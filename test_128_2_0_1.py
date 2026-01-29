import subprocess

def run_command(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    output, _ = process.communicate()
    return output.decode("utf-8")

def main():
    command = "./NewgenMLP Clay 12 8 64 2 0 1"
    num_executions = 10

    for i in range(num_executions):
        print("Execution {}: ".format(i + 1))

        output = run_command(command)
        output_lines = output.strip().split("\n")
        last_three_lines = output_lines[-3:]
        for line in last_three_lines:
            print(line)
        print("=" * 50)

if __name__ == "__main__":
    main()
