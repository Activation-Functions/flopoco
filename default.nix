{
  sources ? import ./npins,
  pkgs ? import sources.nixpkgs { config.allowUnfree = true; },
}:

{
  devShell = pkgs.mkShell {
    name = "flopoco-dev";

    packages = with pkgs; [
      # Used to compile flopoco
      cmake
      pkg-config
      bison
      boost
      flex
      gmp
      libxml2
      blas
      mpfi
      mpfr
      scalp
      sollya
      wcpg

      # Dev utilities
      clang-tools
      gnuplot
      ninja
      nvc
      pre-commit
      rlwrap
    ];

    shellHook = ''
      if [ -e .git/hooks/pre-commit ]; then
        # Remove existing hook
        rm .git/hooks/pre-commit
      fi

      # Install pre-commit hooks
      pre-commit install
    '';
  };
}
