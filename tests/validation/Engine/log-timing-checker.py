import argparse
from datetime import datetime, timezone

EXECUTION_TIME = datetime.now(timezone.utc) # used for default year
CSV_DELIMITER = ";"
LOG_DATE_FORMAT = r"%b %d %H:%M:%S.%f %Y" # Example: "Mar 17 23:59:50.744"
FRAME_KEYWORD = "Frame" # TODO: Allow changing with parameters

# FIXME: At the moment this code can produce misleading results with wrong data assigned to wrong csv header
# TODO: There is a possibility to have more than a single result from a single timestamp
# FIXME: There is no CSV_DELIMITER printed for some of the results, when there is situation as above
# CONCLUSION: Move from using static printing to pandas DataFrame, but think about how to deal with 0.0 delay first


def print_csv_header(filenames: list): # csv header
    print("datetime", end=CSV_DELIMITER)
    for filename in filenames:
        print(f"{filename}-frame", end=CSV_DELIMITER)
        print(f"{filename}-delta")


def print_csv_results(results: dict):
    for timestamp in results.keys():
            first = True
            for result in results[timestamp]:
                filename, frame, delta = result
                if first:
                    print(timestamp, end=CSV_DELIMITER)
                print(f"{filename}-{frame}", end=CSV_DELIMITER)
                print(delta, end=CSV_DELIMITER if first else "")
                if first:
                    first = False
            print()


def process_all_files(filenames: list, year: str):
    # results = {"datetime": [(filename,frame,delta),(filename,frame,delta),...]}
    results = {}
    for filename in filenames:
        with open(filename, "r") as input_file:
            previous_dt = ""
            for line in input_file:
                line_string = line.strip()
                if FRAME_KEYWORD in line_string:
                    frame_number = int(line_string.split(" ")[-1])
                    datetime_string = line_string[:19] + f"000 {year}" # padded for strptime
                    if previous_dt: # set previous and current for delta calculation
                        previous_dt = current_dt
                        current_dt = datetime.strptime(datetime_string, LOG_DATE_FORMAT)
                    else: # first execution, set both previous and current to the same value
                        current_dt = datetime.strptime(datetime_string, LOG_DATE_FORMAT)
                        previous_dt = current_dt
                    delta = current_dt - previous_dt

                    # building results dictionary
                    current_text_dt = datetime.strftime(current_dt, r"%Y-%m-%d_%H:%M:%S.%f")
                    if current_text_dt in results.keys():
                        results[current_text_dt] += [(filename, frame_number, delta.microseconds / 1000)]
                    else:
                        results[current_text_dt] = [(filename, frame_number, delta.microseconds / 1000)]
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Use `python log-timing-checker.py [ARGS] > output.csv` to save the output to a file.")
    parser.add_argument("--filenames", nargs="+", required=True, help="provide at least one file to process, use space as delimiter for more files")
    parser.add_argument("--year", required=False, help=f"default: current year ({EXECUTION_TIME.year})")
    args = parser.parse_args()

    year = args.year if args.year else str(EXECUTION_TIME.year)

    print_csv_header(args.filenames)
    results = process_all_files(args.filenames, year)
    print_csv_results(results)
