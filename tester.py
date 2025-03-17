import subprocess

def test_small_input():
    LARGE_STRING = "R" * 1024 + "rohan"
    result = subprocess.run(["./rshell"], input=f"{LARGE_STRING}\n", capture_output=True, text=True)
    
    print(result.stdout)
    

def main():
    test_small_input()

if __name__ == "__main__":
    main()
