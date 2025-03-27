import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Test")
    parser.add_argument("-m", type=float, required=False, help="Mutation rate.")
    parser.add_argument("-ma", type=str, help="Filename for the mutation rate map.")
    args = parser.parse_args()
    print(args.ma)
