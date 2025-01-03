import sys
import re
import ctypes

# Define a mapping from Java types to JNI types and ctypes
type_map = {
    "void": ("V", ctypes.c_void_p),
    "boolean": ("Z", ctypes.c_bool),
    "byte": ("B", ctypes.c_byte),
    "char": ("C", ctypes.c_char),
    "short": ("S", ctypes.c_short),
    "int": ("I", ctypes.c_int),
    "long": ("J", ctypes.c_long),
    "float": ("F", ctypes.c_float),
    "double": ("D", ctypes.c_double),
    "String": ("Ljava/lang/String;", ctypes.c_void_p)  # Strings are represented as jobject (void pointer)
}

def get_jni_signature(return_type, params):
    """
    Generate the JNI signature for a given method.

    :param return_type: Return type of the method
    :param params: List of tuples representing parameter types and their nullability
                   e.g., ("String", True) for a nullable String parameter
    :return: JNI signature string
    """
    def get_type_signature(param_type):
        if param_type in type_map:
            return type_map[param_type][0]
        elif param_type.endswith("[]"):
            return "[" + get_type_signature(param_type[:-2])
        else:
            return "L" + param_type.replace('.', '/') + ";"

    param_signatures = [get_type_signature(param_type) for param_type, _ in params]
    return_signature = get_type_signature(return_type)

    return f"({''.join(param_signatures)}){return_signature}"

def get_ctypes_signature(return_type, params):
    """
    Generate the ctypes function signature for a given method.

    :param return_type: Return type of the method
    :param params: List of tuples representing parameter types and their nullability
                   e.g., ("String", True) for a nullable String parameter
    :return: ctypes function signature
    """
    def get_ctypes_type(param_type):
        if param_type in type_map:
            return type_map[param_type][1]
        elif param_type.endswith("[]"):
            return ctypes.POINTER(get_ctypes_type(param_type[:-2]))
        else:
            return ctypes.c_void_p  # Custom types are represented as jobject (void pointer)

    param_types = [get_ctypes_type(param_type) for param_type, _ in params]
    return_type = get_ctypes_type(return_type)

    return ctypes.CFUNCTYPE(return_type, ctypes.c_void_p, ctypes.c_void_p, *param_types)

def parse_method_declaration(declaration):
    """
    Parse the Java method declaration to extract the method name, return type, and parameters.

    :param declaration: Java method declaration string
    :return: Tuple containing the method name, return type, and list of parameters with their nullability
    """
    method_pattern = re.compile(r'public\s+static\s+native\s+(\w+)\s+(\w+)\((.*)\)')
    match = method_pattern.match(declaration)
    if not match:
        raise ValueError("Invalid method declaration")

    return_type = match.group(1)
    method_name = match.group(2)
    params_str = match.group(3)

    params = []
    if params_str:
        param_pattern = re.compile(r'(@Nullable\s+)?(\w+)\s+(\w+)')
        for param_match in param_pattern.finditer(params_str):
            nullable = param_match.group(1) is not None
            param_type = param_match.group(2)
            params.append((param_type, nullable))

    return method_name, return_type, params

def generate_function_signature(declaration):
    """
    Generate the JNI and ctypes function signatures for a given method declaration.

    :param declaration: Java method declaration string
    :return: Tuple containing the JNI signature and the ctypes function signature
    """
    method_name, return_type, params = parse_method_declaration(declaration)
    jni_signature = get_jni_signature(return_type, params)
    ctypes_signature = get_ctypes_signature(return_type, params)

    return method_name, jni_signature, ctypes_signature

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python jni_signature.py \"<Java method declaration>\"")
        sys.exit(1)

    declaration = sys.argv[1]
    method_name, jni_signature, ctypes_signature = generate_function_signature(declaration)
    print(f"Method Name: {method_name}")
    print(f"JNI Signature: {jni_signature}")
    print(f"ctypes Signature: {ctypes_signature}")