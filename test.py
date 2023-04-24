import csv
import os
import subprocess
import sys


data = list(csv.reader(open("tests/argument/tests.csv")))
failed=0
passed=0


print("-------ARGUMENT tests----------")
for test in data:
    args = test[1]
    program = args.split(" ")
    program = os.getcwd() + "/"  + program[0]
    print("Test: " + test[0])
    print("")
    print("arguments: "+ test[1])
    sys.stdout.write("output: ")
    sys.stdout.flush()
    process  = subprocess.run(args=program)

    if process.returncode == int(test[2]):
        print("\nTest passed")
        passed += 1
    else:
        print("\nTest failed")
        failed += 1
    print("---------------------------------")


print("")
print("Passed: " + str(passed))
print("Failed: " + str(failed))
print("Total: " + str(passed + failed))