#!/usr/bin/env python3
"""Compile public C++ contracts, including deliberate errors, without linking."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
compiler = shlex.split(os.environ.get('CXX', 'c++'))
bake_env = subprocess.check_output(['bake', 'env'], text=True)
bake_home = next(line.split('=', 1)[1] for line in bake_env.splitlines() if line.startswith('BAKE_HOME='))
prefix = '''#include <siecs.h>
#include <memory>
struct Position { int value; };
struct Frozen {};
struct Damage { int amount; };
struct Other { int amount; };
'''
cases = {
    'generic_query_and_builder_chains': (True, '', '''
void check(ecs::entity target) {
    auto q=ecs::query<Position&>().without<Frozen>().build();
    q.each([](auto& p){++p.value;});
    auto s=ecs::system("s").with<Position>().main_thread().each([](Position&){});
    s.disable(); s.enable(); s.run();
    auto o=ecs::observe<Damage>().with<Position>().target(target).each(
        [state=std::make_unique<int>(1)](ecs::event<Damage> event) {(void)state; (void)event->amount;});
    o.remove();
}'''),
    'generic_lambda_needs_signature': (False, 'use explicitly typed parameters', '''
void check(){ecs::query().with<Position>().each([](auto& p){(void)p;});}'''),
    'generic_signature_mismatch': (False, 'declared query signature', '''
void check(){ecs::query<const Position&>().each([](Position& p){++p.value;});}'''),
    'payload_mismatch': (False, 'payload must match', '''
void check(){ecs::observe<Damage>().each([](ecs::event<Other> event){(void)event;});}'''),
    'resource_get_is_read_only': (False, '', '''
void check(){ecs::resource<Position>().get().value=3;}'''),
    'const_resource_cannot_edit': (False, '', '''
void check(){ecs::resource<const Position>().edit().value=3;}'''),
    'const_resource_cannot_set': (False, '', '''
void check(){ecs::resource<const Position>().set(Position{3});}'''),
    'const_resource_cannot_remove': (False, '', '''
void check(){ecs::resource<const Position>().remove();}'''),
    'entity_get_is_read_only': (False, '', '''
void check(ecs::entity e){e.get<Position>().value=3;}'''),
    'old_entrypoints_are_removed': (False, '', '''
void check(){ecs::entity::create(); ecs::resource_handle<Position>(); ecs::query().build_handle();}'''),
}
failed = 0
with tempfile.TemporaryDirectory(prefix='siecs-cpp-contracts-') as directory:
    source = Path(directory) / 'contract.cpp'
    for name, (success, diagnostic, code) in cases.items():
        source.write_text(prefix + code)
        command = compiler + ['-std=c++20', '-fsyntax-only', '-I'+str(ROOT/'include'), '-I'+str(Path(bake_home)/'include'), str(source)]
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        passed = (result.returncode == 0) == success and (not diagnostic or diagnostic in result.stdout)
        print(('PASS' if passed else 'FAIL') + ': ' + name)
        if not passed:
            failed += 1
            print(result.stdout)
print(f'Compile contracts: {len(cases)-failed} PASS, {failed} FAIL')
raise SystemExit(bool(failed))
