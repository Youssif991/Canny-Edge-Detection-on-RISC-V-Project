import sys
import os
import traceback

try:
    # Set up paths to import m.css modules
    sys.path.append(os.path.abspath('docs/m.css/documentation'))
    import doxygen

    config = doxygen.default_config.copy()
    config.update({
        'DOXYFILE': '../Doxyfile-mcss',
        'SHOW_UNDOCUMENTED': True
    })

    state = doxygen.State(config)
    doxygen.parse_doxyfile(state, 'Doxyfile-mcss')

    xml_input = os.path.join(state.basedir, state.doxyfile['OUTPUT_DIRECTORY'], state.doxyfile['XML_OUTPUT'])
    import glob
    xml_files_metadata = glob.glob(os.path.join(xml_input, "*.xml"))
    for file in xml_files_metadata:
        doxygen.extract_metadata(state, file)

    doxygen.postprocess_state(state)

    output_lines = []
    # Test parse_xml on canny_theory.xml
    target_file = os.path.join(xml_input, 'canny_theory.xml')
    parsed = doxygen.parse_xml(state, target_file)
    output_lines.append(f"parse_xml result for canny_theory.xml: {parsed}")
    if parsed:
        output_lines.append(f"Compound ID: {parsed.compound.id}")
        output_lines.append(f"Compound Kind: {parsed.compound.kind}")
        output_lines.append(f"Compound Name: {parsed.compound.name}")

    with open('scratch/test_output.txt', 'w') as f:
        f.write('\n'.join(output_lines))
    print("Done writing success output.")
except Exception as e:
    with open('scratch/test_output.txt', 'w') as f:
        f.write("ERROR OCCURRED:\n")
        f.write(traceback.format_exc())
    print("Done writing error output.")
