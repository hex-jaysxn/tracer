BUILD libVCTracerMenu.so WITHOUT AN NDK (phone browser is enough)

1. Make a free account at github.com and create a NEW repository (any name, Public or Private).
2. Upload ALL files from this zip keeping the folder structure:
      VCTracerMenu.cpp
      build.sh
      .github/workflows/build.yml
   (Tip: on the repo page use  Add file > Upload files.  If your phone can't upload the
    hidden .github folder, use  Add file > Create new file, type the name
    .github/workflows/build.yml and paste the contents of build.yml.)
3. Open the repo's "Actions" tab. The "Build libVCTracerMenu.so" run starts by itself
   (or press "Run workflow").
4. When it shows a green check, open the run and download the artifact "libVCTracerMenu".
   Unzip it -> libVCTracerMenu.so
5. Put libVCTracerMenu.so in your mods folder (replace the old one, delete
   libVCTracerSolid.so if present), keep libVC_Tracer_Nezuko.so, and use VC_Tracer_Menu.lua.
