import os

OUTPUT_DIR_NAME = 'output'

RULES = [
    ('.c', '.o', '$(CC) -c $(ALL_CFLAGS) $< -o $@', 'OBJECTS'),
    ('.S', '.o', '$(CC) -c $(ALL_ASFLAGS) $< -o $@', 'OBJECTS'),
]

def main(sources_file):
    sources_path = os.path.dirname(os.path.abspath(sources_file))
    gforce_base_path = os.path.dirname(sources_path)
    project_name = os.path.basename(sources_path)

    makedirs = set()
    rules = set()
    out_groups = {}

    with open(sources_file, 'r') as sources_fh:
        for line in sources_fh:
            source = line.rstrip()

            if not source:
                continue

            source_file = os.path.abspath(source)

            # It's a file outside of this project
            if os.path.commonprefix([source_file, sources_path]) != sources_path:
                other_project_name = os.path.split(os.path.relpath(source_file, start=gforce_base_path))[0]
                other_project_dir = os.path.join(gforce_base_path, other_project_name)

                input_file = os.path.relpath(source_file, start=other_project_dir)
                input_dir_pattern = os.path.relpath(other_project_dir, start=sources_path)
                output_dir_pattern = os.path.join(input_dir_pattern, OUTPUT_DIR_NAME, project_name)

            else:
                input_file = os.path.relpath(source_file, start=sources_path)
                input_dir_pattern = ''
                output_dir_pattern = OUTPUT_DIR_NAME

            for rule_in_ext, rule_out_ext, recipe, out_group in RULES:
                in_base, in_ext = os.path.splitext(input_file)

                if in_ext != rule_in_ext:
                    continue

                rules.add((os.path.join(input_dir_pattern, '%' + rule_in_ext),
                           os.path.join(output_dir_pattern, '%' + rule_out_ext), recipe))

                makedirs.add(output_dir_pattern)

                base_dir = os.path.commonprefix([input_file, output_dir_pattern])
                obj = os.path.join(output_dir_pattern, os.path.relpath(in_base, start=base_dir) + rule_out_ext)

                out_groups.setdefault(out_group, []).append(obj)


    with open(os.path.join(sources_path, 'make.rules'), 'w') as fh:
        for var_name, objs in out_groups.items():
            fh.write(var_name + '=' +' '.join(objs) + '\n\n')

        for input_file, output_file, make_rule in rules:
            fh.write('%s : %s\n' % (output_file, input_file))
            fh.write('\t%s\n\n' % make_rule)

    for makedir in makedirs:
        os.makedirs(makedir, exist_ok=True)


if __name__ == '__main__':
    main('sources.mk')
