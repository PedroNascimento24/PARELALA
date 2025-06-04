import json

class DatasetParser:
    """
    A class to parse dataset files and extract information into structured data.
    Attributes:
        general_info (dict): General information about the dataset.
        projects (list): List of projects with their details.
        precedence_relations (list): List of precedence relations between jobs.
        duration_resources (list): List of job durations and resource requirements.
        resource_availability (list): List of available resources and their quantities.
    Methods:
        parse_dataset(file_path):
            Parses the dataset file and extracts information into structured data.
        _process_section(section, line):
            Processes a line of text based on the current section and updates the corresponding attribute.
    """
    def __init__(self):
        self.general_info = {}
        self.projects = []
        self.precedence_relations = []
        self.duration_resources = []
        self.resource_availability = []

    def parse_dataset(self, file_path):
        with open(file_path, 'r') as file:
            lines = file.readlines()

        section = None
        for line in lines:
            line = line.strip()
            
            if line.startswith('#'):
                if 'General Information' in line:
                    section = 'general_info'
                elif 'Projects summary' in line:
                    section = 'projects'
                elif 'Precedence relations' in line:
                    section = 'precedence_relations'
                elif 'Duration and resources' in line:
                    section = 'duration_resources'
                elif 'Resource availability' in line:
                    section = 'resource_availability'
            elif line.startswith('*') or not line:
                continue
            else:
                self._process_section(section, line)

        return {
            'general_info': self.general_info,
            'projects': self.projects,
            'precedence_relations': self.precedence_relations,
            'duration_resources': self.duration_resources,
            'resource_availability': self.resource_availability
        }

    def _process_section(self, section, line):
        if section == 'general_info' and ':' in line:
            key, value = [part.strip() for part in line.split(':')]
            number = ''.join(filter(str.isdigit, value))
            if key and value:
                if 'projects' in key.lower():
                    self.general_info['projects'] = int(value)
                elif 'jobs' in key.lower():
                    self.general_info['jobs'] = int(value)
                elif 'horizon' in key.lower():
                    self.general_info['horizon'] = int(value)
                elif 'nonrenewable' in key.lower():
                    if 'resources' not in self.general_info:
                        self.general_info['resources'] = {}
                    self.general_info['resources']['nonrenewable'] = int(number)
                elif 'renewable' in key.lower():
                    if 'resources' not in self.general_info:
                        self.general_info['resources'] = {}
                    self.general_info['resources']['renewable'] = int(number)
                elif 'doubly constrained' in key.lower():
                    if 'resources' not in self.general_info:
                        self.general_info['resources'] = {}
                    self.general_info['resources']['doubly_constrained'] = int(number)

        elif section == 'projects':
            parts = line.split()
            if not('pronr.' in line) and len(parts) == 6:
                self.projects.append({
                    'pronr': int(parts[0]),
                    'jobs': int(parts[1]),
                    'rel_date': int(parts[2]),
                    'duedate': int(parts[3]),
                    'tardcost': int(parts[4]),
                    'MPM_Time': int(parts[5])
                })

        elif section == 'precedence_relations':
            parts = line.split()
            if len(parts) >= 3:
                self.precedence_relations.append({
                    'jobnr': int(parts[0]),
                    'modes': int(parts[1]),
                    'successors': int(parts[2]),
                    'successors_list': [int(x) for x in parts[3:]]
                })

        elif section == 'duration_resources':
            parts = line.split()
            if len(parts) >= 3:
                resource_obj = {f'R{i+1}': int(val) for i, val in enumerate(parts[3:])}
                self.duration_resources.append({
                    'jobnr': int(parts[0]),
                    'mode': int(parts[1]),
                    'duration': int(parts[2]),
                    **resource_obj
                })

        elif section == 'resource_availability':
            parts = line.split()
            if len(parts) == 2:
                self.resource_availability.append({
                    'resource': parts[0],
                    'qty': int(parts[1])
                })

def write_resources_py(data, output_path):
    # Format precedence relations on single lines
    precedence_str = ',\n    '.join(
        f'{{"jobnr": {r["jobnr"]}, "modes": {r["modes"]}, "successors": {r["successors"]}, "successors_list": {r["successors_list"]}}}'
        for r in data['precedence_relations']
    )
    
    # Format duration resources on single lines  
    duration_str = ',\n    '.join(
        f'{{"jobnr": {d["jobnr"]}, "mode": {d["mode"]}, "duration": {d["duration"]}, ' + 
        ', '.join(f'"{k}": {v}' for k,v in d.items() if k.startswith('R')) + '}'
        for d in data['duration_resources']
    )

    # Format resource availability on single lines
    resource_str = ',\n    '.join(
        f'{{"resource": "{r["resource"]}", "qty": {r["qty"]}}}'
        for r in data['resource_availability']
    )

    output = f"""# General Parameters
general_info = {json.dumps(data['general_info'], indent=4)}

# Project Information
projects = {json.dumps(data['projects'], indent=4)}

# Precedence Relations
precedence_relations = [
    {precedence_str}
]

# Duration and Resources
duration_resources = [
    {duration_str}
]

# Resource Availability
resource_availability = [
    {resource_str}
]"""

    with open(output_path, 'w') as f:
        f.write(output)

if __name__ == '__main__':
    dataset_path = 'dataset_2.txt'
    output_path = 'resources_3241.py'
    
    parser = DatasetParser()
    data = parser.parse_dataset(dataset_path)
    write_resources_py(data, output_path)