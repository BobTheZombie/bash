#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare native arithmetic, fallback and an independent pre-JIT Bash build."""
import pathlib, random, shlex, subprocess, sys
shell=str(pathlib.Path(sys.argv[1]).resolve())
reference=str(pathlib.Path(sys.argv[2]).resolve()) if len(sys.argv)>2 else None
r=random.Random(47021)

def run(body, mode='on', exe=shell):
    prefix='' if exe==reference else f'shopt -{"s" if mode=="on" else "u"} arithmetic_jit; '
    return subprocess.run([exe,'--noprofile','--norc','-c',prefix+body],
                          text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

def equivalent(body):
    values=[run(body,'on'),run(body,'off')]
    if reference: values.append(run(body,exe=reference))
    # Same command name (_) and source line counts make error text comparable;
    # the shell executable path is the only permitted normalization.
    def normalized(p):
        return p.returncode,p.stdout,p.stderr.replace(shell,'BASH').replace(reference or shell,'BASH')
    assert all(normalized(p)==normalized(values[0]) for p in values), (body,[normalized(p) for p in values])
    return values[0]

ops=['+','-','*','&','|','^','<<','>>','<','>','<=','>=','==','!=']
def expr(depth=0):
    if depth>=3 or r.randrange(4)==0: return r.choice(['x','y','z',str(r.randrange(256))])
    if r.randrange(5)==0: return r.choice(['-','+','!','~'])+'('+expr(depth+1)+')'
    return '('+expr(depth+1)+r.choice(ops)+expr(depth+1)+')'

# Warm every expression and assert it actually entered native code.
compiled=executed=0
for _ in range(180):
    e=expr()
    body='x=-9223372036854775808; y=18446744073709551615; z=63; '
    body+=f'for ((round=0;round<18;round++)); do printf "%s\\n" "$(({e}))"; x=$((x+123456789)); y=$((y-987654321)); z=$((z-1)); done; '
    equivalent(body)
    p=run(body+'lpmjitctl stats')
    stats=dict(x.split('=') for x in p.stdout.splitlines()[-1].split())
    assert int(stats['executed'])>0, (e,stats)
    compiled+=int(stats['compiled']);executed+=int(stats['executed'])

for assignment in ['x=x+y*7','x+=y*7','x-=y','x*=y','x&=y','x^=y','x|=y','x<<=y','x>>=y','x++','x--','++x','--x']:
    equivalent(f'x=9223372036854775807; y=3; for ((round=0;round<40;round++)); do printf "%s:%s\\n" "$(({assignment}))" "$x"; done')

# Cache must never hold stale variable pointers or values. Change type/value/
# scope after warmup and compare all error/status/side-effect behavior.
cases=[
 'x=3; for ((i=0;i<20;i++)); do : "$((x+1))"; done; x="y+2"; y=7; echo "$((x+1))"',
 'x=3; for ((i=0;i<20;i++)); do : "$((x+1))"; done; x=010; echo "$((x+1))"',
 'x=3; for ((i=0;i<20;i++)); do : "$((x+1))"; done; x=16#ff; echo "$((x+1))"',
 'x=3; for ((i=0;i<20;i++)); do : "$((x+1))"; done; unset x; set -u; echo "$((x+1))"',
 'x=3; for ((i=0;i<20;i++)); do : "$((x++))"; done; readonly x; ((x++)); echo status=$? x=$x',
 'x=3; for ((i=0;i<20;i++)); do : "$((x++))"; done; unset x; declare -a x=(11 22); echo "$((x++))"; declare -p x',
 'x=3; for ((i=0;i<20;i++)); do : "$((x++))"; done; unset x; declare -A x=([0]=11); echo "$((x++))"; declare -p x',
 'x=3; for ((i=0;i<20;i++)); do : "$((x++))"; done; unset x; y=7; declare -n x=y; echo "$((x++))"; echo y=$y',
 'x=3; for ((i=0;i<20;i++)); do : "$((x++))"; done; declare -i x; echo "$((x++))"; echo x=$x',
 'x=3; for ((i=0;i<20;i++)); do : "$((x++))"; done; f() { local x=90; echo "$((x++))" "$x"; }; f; echo "$x"',
 'x=3; for ((i=0;i<20;i++)); do : "$((x++))"; done; (echo "$((x++))" "$x"); echo "$x"',
 'x=3; for ((i=0;i<20;i++)); do : "$((x++))"; done; echo "$(echo "$((x++))" "$x")"; echo "$x"',
 'x=3; for ((i=0;i<20;i++)); do : "$((x+1))"; done; f() { echo "$((x+1))"; }; x=22 f; echo "$x"',
 'RANDOM=47021; for ((i=0;i<40;i++)); do echo "$((RANDOM+RANDOM))"; done',
 'x=3; for ((i=0;i<20;i++)); do : "$((x++))"; done; set -e; x=0; ((x++)); echo unreachable',
 'x=1; trap \'x=99\' DEBUG; for ((i=0;i<20;i++)); do echo "$((x+1))"; done',
 'set -T; f() { local x=9; for ((i=0;i<20;i++)); do : "$((x++))"; done; }; trap \'echo returning\' RETURN; f',
 'x=3; for ((i=0;i<20;i++)); do : "$((x+1))"; done; BASH_COMPAT=51; echo "$((x+1))"',
 'x=3; for ((i=0;i<20;i++)); do : "$((x+1))"; done; set -o posix; echo "$((x+1))"',
 # Unsupported syntax retains interpreter side effects and short-circuiting.
 'x=0; for ((i=0;i<20;i++)); do echo "$((0 && x++))" "$((1 || x++))" "$x"; done',
 'x=0; for ((i=0;i<20;i++)); do echo "$((x++, x ? x*2 : x++))" "$x"; done',
 'x=1; for ((i=0;i<20;i++)); do echo "$((x**3))" "$((x/2))" "$((x%3))"; ((x++)); done',
]
for body in cases: equivalent(body)
# Invalid expressions must never become valid after the hot threshold.
for e in ['x\r+1','1 2','x--1','x+','08','1<<','(x','x/**/y']:
    equivalent('x=3; y=2; for ((i=0;i<20;i++)); do let '+shlex.quote(e)+'; echo status=$?; done')
# Exercise invalidation and disabling, and require real machine code on x86-64.
p=run('x=1; for ((i=0;i<100;i++)); do ((x+=i*3)); done; lpmjitctl stats; lpmjitctl clear; lpmjitctl stats; shopt -u arithmetic_jit; ((x++)); lpmjitctl stats')
assert p.returncode==0,p.stderr
stats=[dict(x.split('=') for x in line.split()) for line in p.stdout.splitlines()]
assert int(stats[0]['executed'])>0 and int(stats[0]['compiled'])>0
assert all(int(v)==0 for k,v in stats[1].items() if k!='enabled')
assert stats[2]['enabled']=='0' and stats[2]['lookups']=='0'
print(f'Integrated arithmetic differential tests passed: {compiled} compiled entries, {executed} native evaluations; state/error/control cases passed')
