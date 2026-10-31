"""Autovalidation du solveur d'audit sur des solutions fermées, AVANT toute comparaison avec TSA."""
import sys, os, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from frame3d import Frame
E=2.1e8; G=E/2.6; b,h=.2,.4; A=b*h; Iy=b*h**3/12; Iz=h*b**3/12; J=1e-3
def beam(L,rel=(),beta=0.0,loads=()):
    f=Frame(); f.nodes={1:(0,0,0),2:(L,0,0)}; f.fix={1:[1]*6,2:[1]*6}
    f.members={1:dict(i=1,j=2,E=E,G=G,A=A,J=J,Iy=Iy,Iz=Iz,beta=beta,released=list(rel),loads=list(loads))}
    return f
L=6.;q=12.
u=lambda v: np.array(v,float)
r=beam(L,loads=[dict(kind='dist',q1=u([0,0,-q]),q2=u([0,0,-q]),a=0,b=L)]).solve()
print('bi-encastrée uniforme  R', r['r'][1][2], 'att', q*L/2, ' M', r['r'][1][4], 'att ±', q*L*L/12)
r=beam(L,rel=[4],loads=[dict(kind='dist',q1=u([0,0,-q]),q2=u([0,0,-q]),a=0,b=L)]).solve()
print('rotule i uniforme      R', r['r'][1][2], 'att', 3*q*L/8, ' M_j', r['r'][2][4], 'att ±', q*L*L/8)
r=beam(L,rel=[4],loads=[dict(kind='dist',q1=u([0,0,0]),q2=u([0,0,-q]),a=0,b=L)]).solve()
print('rotule i triangle 0→q  R', r['r'][1][2], 'att', q*L/10, ' M_j', r['r'][2][4], 'att ±', q*L*L/15)
P=30.;a=2.;bb=L-a
r=beam(L,rel=[4],loads=[dict(kind='point',p=u([0,0,-P]),a=a)]).solve()
print('rotule i ponctuelle    R', r['r'][1][2], 'att', P*bb**2*(2*L+a)/(2*L**3), ' M_j', r['r'][2][4], 'att ±', P*a*bb*(L+a)/(2*L*L))
f=Frame(); f.nodes={1:(0,0,0),2:(L,0,0)}; f.fix={1:[1]*6}; f.members={1:dict(i=1,j=2,E=E,G=G,A=A,J=J,Iy=Iy,Iz=Iz,beta=0,released=[],loads=[])}; f.nodal={2:[0,0,-P,0,0,0]}
r=f.solve(); print('console flèche         ', r['u'][2][2], 'att', -P*L**3/(3*E*Iy), ' rot', r['u'][2][4], 'att', P*L**2/(2*E*Iy))
f.nodal={2:[0,-P,0,0,0,0]}; r=f.solve(); print('console flèche faible  ', r['u'][2][1], 'att', -P*L**3/(3*E*Iz))
f.nodal={2:[0,0,0,5.,0,0]}; r=f.solve(); print('torsion                ', r['u'][2][3], 'att', 5.*L/(G*J))
# section tournée de 90° : charge verticale reprise par Iz
f=Frame(); f.nodes={1:(0,0,0),2:(L,0,0)}; f.fix={1:[1]*6}; f.members={1:dict(i=1,j=2,E=E,G=G,A=A,J=J,Iy=Iy,Iz=Iz,beta=90.,released=[],loads=[])}; f.nodal={2:[0,0,-P,0,0,0]}
r=f.solve(); print('β=90 flèche            ', r['u'][2][2], 'att', -P*L**3/(3*E*Iz))
