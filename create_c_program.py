def escape_for_c_string(input_string):
    # Escape backslashes, double quotes, and newlines for C string literals
    input_string = input_string.replace('\\', '\\\\')  # Escape backslashes
    input_string = input_string.replace('"', '\\"')    # Escape double quotes
    input_string = input_string.replace('\n', '\\n"\\\n"')  # Escape newlines
    return input_string

def generate_c_program(bash_script_path, output_file):
    try:
        with open(bash_script_path, 'r') as bash_file:
            script_content = bash_file.read()

        # Escape the bash script content to be used in a C string
        escaped_script = escape_for_c_string(script_content)

        # C program template
        c_program = f'''
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
extern char **environ;

int main() {{
    char *args[] = {{
        "/bin/bash", "-c",
        "{escaped_script}",
        NULL
    }};

    execve("/bin/bash", args, environ);

    perror("execve");
    return 1;
}}
'''
        # Write the C program to the output file
        with open(output_file, 'w') as output:
            output.write(c_program)
        print(f"C program generated successfully and saved to {output_file}")

    except Exception as e:
        print(f"Error: {e}")

# Specify the path to your installer.sh and output C file
generate_c_program('online_middleware_installer.sh', 'generated_program.c')


#python3 create_c_program.py

#gcc -o MiPhi_aiDAPTIV_Online_Middleware_Installer generated_program.c