"""Extract the production callbacks verbatim for the portable test double harness."""

import argparse
from pathlib import Path


def extract_function(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    closing = opening + 1
    while depth:
        if source[closing] == "{":
            depth += 1
        elif source[closing] == "}":
            depth -= 1
        closing += 1
    return source[start:closing]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    module = Path(__file__).resolve().parents[2]
    source = (module / "src/main/cpp/native_bridge.cpp").read_text()
    callbacks = [
        extract_function(source, "static napi_value MMBufferToNValue(napi_env env, const MMBuffer &value)"),
        extract_function(source, "static napi_value encodeBytes("),
        extract_function(source, "template <typename T>\nnapi_value encodeArray("),
    ]
    args.output.write_text("// Generated verbatim from native_bridge.cpp.\n" + "\n\n".join(callbacks) + "\n")


if __name__ == "__main__":
    main()
