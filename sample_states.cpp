// sample_states.cpp
// this function samples states for backbone and side chain angles, using both
// angle- and distance-sampling methods

#include "sample_states.h"
#include "potential.h"
#include "reprst.h"
#include "residue.h"
#include "smc.h"

void sample_bb_angles(Structure& conf, int position, double(*angles)[3],
		      int numStates, int bbType) {

  // Determine this residue
  int aaIndex = conf._res[position]._type;
  if (aaIndex == -1)
    aaIndex = 1;
  string res = Residue::Name3[aaIndex];

  if (bbType == 1) {

    // Sample from angle database for this residue.
    char **rst;
    int nrow;
    int ncol;
    char *err;
    for (int i = 0; i < numStates; i++) {
      double chosen = drand(0, 1);
      string cmd = "select phi, psi, omega from BBtorsion where residue = '" +
	res + "' and cumprob > " + to_string(chosen) + " limit 1";
    }

  } else if (bbType == 2) {

    // Sample first from bucketed data, then uniformly.
    char **rst;
    int nrow;
    int ncol;
    char *err;
    for (int i = 0; i < numStates; i++) {
      double chosen = drand(0, 1);
      string cmd = "select phi1, phi2, psi1, psi2, omega1, omega2 from BBT_PF where residue = '" +
	res + "' and cumprob > " + to_string(chosen) + " limit 1";
    }

  }  
  else if (bbType == 3) {
// Sample angles near defined (native) conformation
  double native_bbangles[3];
  double tempangles[3];
  for(int i=0; i < 3;i++)
   native_bbangles[i] = angles[0][i];
  for(int i=0;i<numStates;++i){
      double perturbation = drand(0,1);
      angles[i][0] = frand(native_bbangles[0]-(PI/36)*perturbation, native_bbangles[0]+(PI/36)*perturbation);
      angles[i][1] = frand(native_bbangles[1]-(PI/36)*perturbation, native_bbangles[1]+(PI/36)*perturbation);
      angles[i][2] = frand(native_bbangles[2]-(PI/180)*perturbation, native_bbangles[2]+(PI/180)*perturbation);
    
    box_MullerNsample(tempangles,native_bbangles,3,20);
    angles[i][0] = tempangles[0];
    angles[i][1] = tempangles[1];
    angles[i][2] = tempangles[2];
   } 
  }
  else if (bbType == 4) {
    // Sample uniformly at random.

    for (int i = 0; i < numStates; i++) {
      angles[i][0] = frand((-PI), PI);
      angles[i][1] = frand((-PI), PI);
      angles[i][2] = PI;
    }

  }
 
}

void sample_sc_angles(Residue& res, double(*angles)[6],
		      int numStates, int scType) {
  int i,j,resType,numRot;
  int pick;
  SCT tmpSCT;
  
    if(scType == 1 || scType==2){  // continuous representation
    int key;
    double r;
    FIMAP::iterator fiItr;
    resType = res._type;
    if(resType == -1) resType = 1;
    numRot = Rotamer::numRotBond[resType];
    for(i=0;i<numStates;++i){
      r = frand(0,1);
      if((fiItr = SCR::SCRMAP[resType].upper_bound(r))!=SCR::SCRMAP[resType].end()){
	key = fiItr->second;
	for(j=(numRot-1);j>=0;--j){
	  angles[i][j] = (key%100*SC_T_INT + frand(0,SC_T_INT) - 180)*PI/180;
	  key = key/100;
	}
 	
      }
      else {
	cout<<"SCRMAP out of bound, resample the angle. "<<resType<<" "<<r<<" "<<SCR::SCRMAP[resType].size()<<endl;
	exit(0);
	i--;
      }
    }
  }
  else if(scType==3) { // sample the +- 60 degree of the native torsion angles
    double r,a,b;
    resType = res._type;
    numRot = Rotamer::numRotBond[resType];
    for(i=0;i<numStates;++i){
      for(j=0;j<numRot;++j) {
	a = res._scChi[j]-10;
	b = res._scChi[j]+10;
	r = frand(a,b);
	if(r<-180) r = r + 360;
	else if(r>180) r = r - 360;
	angles[i][j] = r*PI/180;
      }
    }
  }
  else
  {
	 cout<<"Wrong Sidechain sampling type!!!"<<endl;
     exit(0);
  }
}

// minimal, maximal distance of bond angle "circle" to end point
void SMC::sample_distance(Atom& b, Atom& c, Atom& B,
			  double theta, double lcd,
			  Atom& p, double lcon,
			  int label, int rem, bool verbose) {
  // Find minimal, maximal distances of point B to bond angle circle
  double lbc = b.dis(c);
  Point e   = c - (c - b) * (lcd * cos(theta) / lbc);
  Point Bp  = B - (c - b) * ((B - e) ^ (c - b) / (c - b).square());
  Point mm  = e + (Bp - e) * (lcd * sin(theta) / Bp.dis(e));
  Point mp  = e - (Bp - e) * (lcd * sin(theta) / Bp.dis(e));
  double md[2];
  int length = rem ;

  md[0] = lcon - mp.dis(B);
  md[1] = lcon - mm.dis(B);
  Atom p1, p2;
  if (verbose)
    cout << "(" << md[0] << ", " << md[1] << "): ";
  
  double x = lcon - minDistcon[label][length];
  double distby = DistconBy[label][length];
  double delby =  DistdelBy[label][length]; 
  double mindeld = minDistdel[label][length];

  int conlower = int(x/distby + 0.00001);
  double x1 = conlower*distby;
  double x2 = x1 + distby;
  double d,y,y1,y2,lowgap,highgap = 0;

  int min_intY = int((md[0] - mindeld)*(1/delby));    // integer lower bound of delta d
  int max_intY = int((md[1] - mindeld)*(1/delby))+1 ;    // integer upper bound of delta d


  double denominator = distby*delby;
  double cdf[32];
  double pdf[32];

  if(md[0] - mindeld <=0)
  {
    min_intY = 0;
    pdf[min_intY] = ((x2-x)*etedCon[label][length][conlower][0] + (x-x1)*etedCon[label][length][conlower+1][0])*(1/distby);    
   lowgap = 0;
  }
  else
  {
    pdf[min_intY] =(etedCon[label][length][conlower][min_intY]*(x2-x)*(delby*(min_intY+1) - md[0]+mindeld) +
           etedCon[label][length][conlower+1][min_intY]*(x-x1)*(delby*(min_intY+1) - md[0]+mindeld) +
           etedCon[label][length][conlower][min_intY+1]*(x2-x)*(md[0]-mindeld-delby*min_intY) +
           etedCon[label][length][conlower+1][min_intY+1]*(x-x1)*(md[0]-mindeld-delby*min_intY) )/ denominator;
   lowgap = delby - (md[0] - mindeld - delby*min_intY);
  }

  if(max_intY>31)
  {
    max_intY = 31;
    pdf[max_intY] = ((x2-x)*etedCon[label][length][conlower][31] + (x-x1)*etedCon[label][length][conlower+1][31])*(1/distby); 
    highgap = 0;
  }
  else
  {
    pdf[max_intY] =(etedCon[label][length][conlower][max_intY-1]*(x2-x)*(delby*max_intY - md[1]+mindeld) +
           etedCon[label][length][conlower+1][max_intY-1]*(x-x1)*(delby*max_intY - md[1]+mindeld) +
           etedCon[label][length][conlower][max_intY]*(x2-x)*(md[1]-mindeld-delby*(max_intY-1)) +
           etedCon[label][length][conlower+1][max_intY]*(x-x1)*(md[1]-mindeld-delby*(max_intY-1)) )/ denominator;
     
    highgap = delby - (mindeld + delby*max_intY - md[1]);
  }


  int floor = min_intY+1;
  pdf[floor] = ((x2-x)*etedCon[label][length][conlower][floor] + (x-x1)*etedCon[label][length][conlower+1][floor])*(1/distby);
  cdf[min_intY] = 0;

  cdf[floor] = (pdf[floor] + pdf[min_intY])*0.5*lowgap; 


  for(int i = floor + 1; i < max_intY;i++)
  {
    pdf[i] = ((x2-x)*etedCon[label][length][conlower][i] + (x-x1)*etedCon[label][length][conlower+1][i])*(1/distby);
    cdf[i] = cdf[i-1] + (pdf[i] + pdf[i-1])*0.5*delby;
  }

  cdf[max_intY] = cdf[max_intY-1] + (pdf[max_intY]+pdf[max_intY-1])*0.5*highgap;


  double randr = frand(0,cdf[max_intY]);

  double c1, c2 = 0;
  if(randr <= cdf[floor])
  {
    c1 = randr;
    c2 = cdf[floor];
    y = md[0] + c1*lowgap/c2;
  }
  else
  {
    for(int i=floor+1; i < max_intY; i++)
    {
      if(randr <= cdf[i])
      {
        c1 = randr- cdf[i-1];
        c2 = cdf[i] - cdf[i-1];
        y = mindeld + delby*i - delby + c1*delby/c2;
        break;
      }
    }
    if(randr <= cdf[max_intY] &&  randr >= cdf[max_intY-1])
    {
      c1 = randr - cdf[max_intY-1];
      c2 = cdf[max_intY] - cdf[max_intY-1];
      y = mindeld + delby*(max_intY-1) + c1*highgap/c2;
    }
  }
  d = lcon - y;
  
  // Find points on bond angle circle corresponding to distances
 // double cgamma = ((B - e).square() + lcd*lcd * sin(theta)*sin(theta) - d*d) /
  //  (2 * Bp.dis(e) * lcd * sin(theta));
  double gamma = acos(((B - e).square() + lcd*lcd * sin(theta)*sin(theta) - d*d) /
		     (2 * Bp.dis(e) * lcd * sin(theta)));
  p1 = e + (Bp - e) * (lcd * sin(theta) * cos(gamma) / Bp.dis(e)) +
    ((Bp - e) * (c - b)) *
    (lcd * sin(theta) * sin(gamma) / (Bp.dis(e) * c.dis(b)));
  p2 = e + (Bp - e) * (lcd * sin(theta) * cos(gamma) / Bp.dis(e)) -
    ((Bp - e) * (c - b)) *
    (lcd * sin(theta) * sin(gamma) / (Bp.dis(e) * c.dis(b)));

  double frand_chose = frand(0,1);
  double threshold = 0.5;
  if(label == 0)
  {
   if(rem <= 4)
   threshold = 0.52;
   else if(rem >4 && rem <= 8)
   threshold = 0.51;
   else
   threshold = 0.5;
  }
  if(frand_chose <= threshold)
  p = p1;
  else
  p = p2;
}
