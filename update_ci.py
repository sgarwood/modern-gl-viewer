import yaml

with open('.circleci/config.yml', 'r') as f:
    config = yaml.safe_load(f)

ubuntu_steps = config['jobs']['build_ubuntu']['steps']
ubuntu_steps[1]['run']['command'] = "sudo apt-get update\nsudo apt-get install --yes ninja-build npm\n"

mock_step = {
    'name': 'Start Mock Backend',
    'background': True,
    'command': 'npx -y @stoplight/prism-cli mock backend/openapi.yaml -p 5000'
}

test_idx = next(i for i, step in enumerate(ubuntu_steps) if isinstance(step, dict) and step.get('run', {}).get('name') == 'Test')
ubuntu_steps.insert(test_idx, {'run': mock_step})

config['jobs']['build_macos'] = {
    'macos': {
        'xcode': '16.0.0'
    },
    'resource_class': 'macos.m1.medium.gen1',
    'steps': [
        'checkout',
        {
            'run': {
                'name': 'Install build tools',
                'command': 'brew install cmake ninja node'
            }
        },
        {
            'run': {
                'name': 'Configure',
                'command': 'cmake -S . -B build/ci -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DMGV_BUILD_GLFW_APP=OFF -DMGV_BUILD_QT_APP=OFF -DMGV_BUILD_QML_APP=OFF -DMGV_WARNINGS_AS_ERRORS=ON'
            }
        },
        {
            'run': {
                'name': 'Build',
                'command': 'cmake --build build/ci --parallel'
            }
        },
        {
            'run': {
                'name': 'Start Mock Backend',
                'background': True,
                'command': 'npx -y @stoplight/prism-cli mock backend/openapi.yaml -p 5000'
            }
        },
        {
            'run': {
                'name': 'Test',
                'command': 'sleep 5 && ctest --test-dir build/ci --output-on-failure'
            }
        }
    ]
}

ubuntu_steps[test_idx+1]['run']['command'] = "sleep 5 && ctest --test-dir build/ci --output-on-failure"

if 'build_macos' not in config['workflows']['build_matrix']['jobs']:
    config['workflows']['build_matrix']['jobs'].append('build_macos')

with open('.circleci/config.yml', 'w') as f:
    yaml.dump(config, f, sort_keys=False, default_flow_style=False)
