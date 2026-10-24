  VER="1.0.2" 
  git tag -d v${VER} && git push origin --delete v${VER}
  git tag -a v${VER} -m "Release ChromaZ v${VER}" && git push origin v${VER} --tags
  
  tree -a -n --gitignore -I .git -o data/codebase.md
  find README.md README_RU.md src .github/workflows chocolatey -type f ! -name "*.ico" ! -name "*.svg" | while read -r file; do echo -e "\n### Файл: \`$file\`\n"; echo '```'; cat "$file"; echo ; echo '```'; done >> data/codebase.md
