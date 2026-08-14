#!/usr/bin/env python3
"""
Ghidra MCP Bridge Client
------------------------
Utility script for communicating with a running Ghidra MCP server or headless analyzer.
Enables automated decompilation querying, symbol resolution, and function export.
"""

import sys
import json
import argparse
import urllib.request
import urllib.error

DEFAULT_PORT = 13370
DEFAULT_HOST = "127.0.0.1"

def send_ghidra_command(endpoint, data=None, host=DEFAULT_HOST, port=DEFAULT_PORT):
    url = f"http://{host}:{port}/{endpoint.lstrip('/')}"
    try:
        if data is not None:
            payload = json.dumps(data).encode("utf-8")
            req = urllib.request.Request(url, data=payload, headers={"Content-Type": "application/json"})
        else:
            req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=10) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.URLError as e:
        return {"status": "error", "message": f"Could not connect to Ghidra MCP server at {url}: {e}"}

def decompile_function(address_or_name, host=DEFAULT_HOST, port=DEFAULT_PORT):
    return send_ghidra_command("decompile", {"target": address_or_name}, host, port)

def get_symbols(filter_term=None, host=DEFAULT_HOST, port=DEFAULT_PORT):
    return send_ghidra_command("symbols", {"filter": filter_term or ""}, host, port)

def rename_symbol(address, new_name, host=DEFAULT_HOST, port=DEFAULT_PORT):
    return send_ghidra_command("rename", {"address": address, "new_name": new_name}, host, port)

def main():
    parser = argparse.ArgumentParser(description="Ghidra MCP Bridge CLI")
    parser.add_argument("--host", default=DEFAULT_HOST, help="Ghidra MCP server host")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="Ghidra MCP server port")
    subparsers = parser.add_subparsers(dest="command", help="Command to execute")

    decompile_p = subparsers.add_parser("decompile", help="Decompile function at address or by name")
    decompile_p.add_argument("target", help="Address (e.g. 0x0150) or function name")

    symbols_p = subparsers.add_parser("symbols", help="List symbols in program")
    symbols_p.add_argument("--filter", default="", help="Filter symbols by prefix or substring")

    rename_p = subparsers.add_parser("rename", help="Rename symbol at address")
    rename_p.add_argument("address", help="Target address")
    rename_p.add_argument("name", help="New symbol name")

    args = parser.parse_args()

    if args.command == "decompile":
        res = decompile_function(args.target, args.host, args.port)
        print(json.dumps(res, indent=2))
    elif args.command == "symbols":
        res = get_symbols(args.filter, args.host, args.port)
        print(json.dumps(res, indent=2))
    elif args.command == "rename":
        res = rename_symbol(args.address, args.name, args.host, args.port)
        print(json.dumps(res, indent=2))
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
