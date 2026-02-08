#!/usr/bin/env python3
"""
Interactive Python client for the RCCService HTTP API.

This client supports the new HTTP-based API (Linux experimental version)
which uses JSON over HTTP instead of SOAP.

Run without arguments for interactive mode, or with arguments for command-line mode.

Command-line usage:
    python rcc_http_client.py <base_url> <method> [json_data]

Interactive mode:
    Run the script and follow the prompts.
"""

import sys
import json
import requests

def make_request(base_url, endpoint, data=None):
    """Make a POST request to the RCCService API."""
    url = f"{base_url.rstrip('/')}/{endpoint.lstrip('/')}"
    headers = {'Content-Type': 'application/json'}

    try:
        if data:
            response = requests.post(url, json=data, headers=headers)
        else:
            response = requests.post(url, headers=headers)

        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        print(f"Error: {e}")
        return None

def get_user_input(prompt, default=None):
    """Get user input with optional default."""
    if default:
        user_input = input(f"{prompt} (default: {default}): ").strip()
        return user_input if user_input else default
    else:
        return input(f"{prompt}: ").strip()

def interactive_mode():
    """Run the client in interactive mode."""
    print("Welcome to the RCCService HTTP Client!")
    base_url = get_user_input("Enter the base URL of the RCCService (e.g., http://localhost:8080)")

    menu = """
Available methods:
1. HelloWorld
2. GetVersion
3. GetStatus
4. OpenJob
5. Execute
6. CloseJob
7. BatchJob
8. RenewLease
9. GetExpiration
10. GetAllJobs
11. CloseExpiredJobs
12. CloseAllJobs
13. Diag
0. Exit
"""

    while True:
        print(menu)
        choice = get_user_input("Select an option (0-13)")

        if choice == '0':
            print("Goodbye!")
            break
        elif choice == '1':
            result = make_request(base_url, 'HelloWorld')
        elif choice == '2':
            result = make_request(base_url, 'GetVersion')
        elif choice == '3':
            result = make_request(base_url, 'GetStatus')
        elif choice == '4':
            job_id = get_user_input("Job ID")
            expiration = float(get_user_input("Expiration in seconds", "300"))
            category = int(get_user_input("Category", "0"))
            cores = float(get_user_input("Cores", "1"))
            script_name = get_user_input("Script name")
            script_code = get_user_input("Script code")
            data = {
                "job": {
                    "id": job_id,
                    "expirationInSeconds": expiration,
                    "category": category,
                    "cores": cores
                },
                "script": {
                    "name": script_name,
                    "script": script_code
                }
            }
            result = make_request(base_url, 'OpenJob', data)
        elif choice == '5':
            job_id = get_user_input("Job ID")
            script_name = get_user_input("Script name")
            script_code = get_user_input("Script code")
            data = {
                "jobId": job_id,
                "script": {
                    "name": script_name,
                    "script": script_code
                }
            }
            result = make_request(base_url, 'Execute', data)
        elif choice == '6':
            job_id = get_user_input("Job ID")
            data = {"jobId": job_id}
            result = make_request(base_url, 'CloseJob', data)
        elif choice == '7':
            job_id = get_user_input("Job ID")
            expiration = float(get_user_input("Expiration in seconds", "300"))
            category = int(get_user_input("Category", "0"))
            cores = float(get_user_input("Cores", "1"))
            script_name = get_user_input("Script name")
            script_code = get_user_input("Script code")
            data = {
                "job": {
                    "id": job_id,
                    "expirationInSeconds": expiration,
                    "category": category,
                    "cores": cores
                },
                "script": {
                    "name": script_name,
                    "script": script_code
                }
            }
            result = make_request(base_url, 'BatchJob', data)
        elif choice == '8':
            job_id = get_user_input("Job ID")
            expiration = float(get_user_input("New expiration in seconds"))
            data = {
                "jobId": job_id,
                "expirationInSeconds": expiration
            }
            result = make_request(base_url, 'RenewLease', data)
        elif choice == '9':
            job_id = get_user_input("Job ID")
            data = {"jobId": job_id}
            result = make_request(base_url, 'GetExpiration', data)
        elif choice == '10':
            result = make_request(base_url, 'GetAllJobs')
        elif choice == '11':
            result = make_request(base_url, 'CloseExpiredJobs')
        elif choice == '12':
            result = make_request(base_url, 'CloseAllJobs')
        elif choice == '13':
            diag_type = int(get_user_input("Diag type", "0"))
            job_id = get_user_input("Job ID (optional)", "")
            data = {"type": diag_type}
            if job_id:
                data["jobID"] = job_id
            result = make_request(base_url, 'Diag', data)
        else:
            print("Invalid choice. Please try again.")
            continue

        if result is not None:
            print("Response:")
            print(json.dumps(result, indent=2))
        else:
            print("Request failed.")

        input("\nPress Enter to continue...")

def main():
    if len(sys.argv) == 1:
        # No arguments, run interactive mode
        interactive_mode()
    elif len(sys.argv) >= 3:
        # Command-line mode
        base_url = sys.argv[1]
        method = sys.argv[2]

        data = None
        if len(sys.argv) > 3:
            try:
                data = json.loads(sys.argv[3])
            except json.JSONDecodeError as e:
                print(f"Invalid JSON in arguments: {e}", file=sys.stderr)
                sys.exit(1)

        result = make_request(base_url, method, data)
        if result is not None:
            print(json.dumps(result, indent=2))
        else:
            sys.exit(1)
    else:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()
