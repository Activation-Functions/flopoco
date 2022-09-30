#include <cmath>
#include "fragment.h"
#include "stdfragment.h"
#include "logfragment.h"

struct FragDesc
{
  Fragment* fragment;
  double area;
};

Fragment* explore(int explore_size)
{
  /* "cette fonction est l� car potentiellement utile, mais SANS AUCUNE GARANTIE,
     ni explicite ni implicite, y compris les garanties [...] d'adaptation dans
     un but sp�cifique", de retour d'une quelconque valeur ou de non destruction
     de l'ordinateur sur laquelle elle est ex�cut�e */
  // exception : il est garanti que la fonction plante quand explore_size est assez petit
  /* cette fonction ne d�salloue pas la grande quantit� de m�moire qu'elle utilise !
     (il faudrait compter les r�f�rences sur les objets pour �a, ou ne pas allouer
      de m�moire du tout et copier les objets au lieu d'utiliser des pointeurs) */

  // taille maximale d'un morceau (� part le dernier)
  const int max_frag_size = 10;
  int accuracy;     // pr�cision interne explor�e
  int min_length;   // accuracy / 2
  int length;       // longueur des morceaux pour laquelle on recherche l'optimum
  int i, j, k, length2;
  double max_error;
  int max_error_idx;
  int min_max_error_idx; // l'erreur maximale avec un seul morceau
  int max_max_error_idx; // l'erreur maximale... maximale pour que la pr�cision soit atteinte
  FragDesc optimum;
  optimum.fragment = 0;

  /* d�coupages d'aire minimale 
     - de pr�cision = 2�me argment
     - 1er argument = int(log2(erreur_max) * 10) */
  FragDesc* opt_fragment[100];
  max_max_error_idx = 10 - 1;

  for (accuracy = explore_size + 3; accuracy < explore_size + 9; accuracy++) {
    min_length = accuracy / 2;
    cout << "    Exploring cuttings, internal precision " << accuracy << endl;
    for (i = 0; i < 100; i++) {
      opt_fragment[i] = new FragDesc[accuracy];
      for (length = min_length; length <= accuracy - 1; length++)
	opt_fragment[i][length].fragment = 0;
    }

    /* initialise les tables d'optimaux avec les d�coupages ne contenant
       qu'un seul morceau, de longueur de l'ordre de accuracy / 2 */
    {
      FragDesc min_fd;
      min_fd.fragment = new StdFragment(min_length, 0);
      min_fd.fragment->prepare(min_fd.area, max_error, false, accuracy, false);
      min_max_error_idx = static_cast<int>(floor(log(max_error) / log(2.0) * 10));
      for (i = min_max_error_idx; i <= max_max_error_idx; i++)
	opt_fragment[i][min_length] = min_fd;
    }

    for (length = min_length + 1; length <= min_length + 8; length++) {
      FragDesc fd;
      fd.fragment = new StdFragment(length, 0);
      fd.fragment->prepare(fd.area, max_error, false, accuracy, false);
      max_error_idx = static_cast<int>(floor(log(max_error) / log(2.0) * 10));
      for (i = max_error_idx; i <= max_max_error_idx; i++)
	opt_fragment[i][length] = fd;
    }

    // la grosse boucle
    for (length = min_length + 3; length <= accuracy - 1; length++) {
      // recherche de l'optimal par programmation dynamique
      for (i = min_max_error_idx; i <= max_max_error_idx; i++) {
	for (length2 = (min_length > length - max_frag_size ? min_length : length - max_frag_size); length2 <= length - 3; length2++) {
	  for (j = 0; j <= i; j++) {
	    if (opt_fragment[j][length2].fragment != 0) {
	      FragDesc fd;
	      // essaie avec un morceau standard
	      if (length != accuracy - 1) {
		fd.fragment = new StdFragment(length - length2, opt_fragment[j][length2].fragment);
		fd.fragment->prepare(fd.area, max_error, false, accuracy, false);
		max_error_idx = static_cast<int>(floor(log(max_error) / log(2.0) * 10));
		for (k = max_error_idx; k <= max_max_error_idx &&
		     (opt_fragment[k][length].fragment == 0 ||
		      opt_fragment[k][length].area > fd.area); k++)
		  opt_fragment[k][length] = fd;
	      }
	      // essaie avec un morceau avec table de log
	      fd.fragment = new LogFragment(length - length2, opt_fragment[j][length2].fragment);
	      fd.fragment->prepare(fd.area, max_error, length == accuracy - 1, accuracy, false);
	      max_error_idx = static_cast<int>(floor(log(max_error) / log(2.0) * 10));
	      for (k = max_error_idx; k <= max_max_error_idx &&
		   (opt_fragment[k][length].fragment == 0 ||
		    opt_fragment[k][length].area > fd.area); k++)
		opt_fragment[k][length] = fd;
	    } // if (opt_fragment[j][length2].fragment != 0) {
	  }
	}
      }
    } // for (i = min_max_error_idx; i <= max_max_error_idx; i++) {
    {
      // bilan
      FragDesc& fd = opt_fragment[max_max_error_idx][accuracy - 1];
      if (fd.fragment != 0 && (optimum.fragment == 0 || fd.area < optimum.area)) {
	cout << "    Optimal result for this precision, area = " << fd.area << endl;
	optimum = fd;
      }
    }
    max_max_error_idx += 10;
  } // for (int accuracy = explore_size + 3; accuracy < explore_size + 10; accuracy++)
  return optimum.fragment;
}
