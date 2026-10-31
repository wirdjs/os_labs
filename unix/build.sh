#!/bin/sh


die() {
    echo "build.sh: ошибка: $1" >&2
    exit "${2:-1}"
}


tmpdir=
cleanup() {
    if [ -n "$tmpdir" ]; then
        rm -rf -- "$tmpdir"
    fi
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 131' QUIT
trap 'exit 141' PIPE
trap 'exit 143' TERM
trap 'exit 138' USR1
trap 'exit 140' USR2


[ "$#" -eq 1 ] || die "использование: $0 <исходный_файл>" 2
src=$1
[ -f "$src" ] || die "файл '$src' не найден" 3
[ -r "$src" ] || die "файл '$src' недоступен для чтения" 3

src_abs_dir=$(cd -- "$(dirname -- "$src")" && pwd) \
    || die "нет доступа к каталогу исходника" 3

out_name=$(awk '
    /^[ \t]*(\/\*|\/\/|%|\*).*Output:/ {
        sub(/^.*Output:[ \t]*/, "")
        sub(/[ \t]*(\*\/)?[ \t]*$/, "")
        print
        exit
    }' "$src")
[ -n "$out_name" ] || die "в файле нет комментария с 'Output:'" 4
case "$out_name" in
    */* | .. | . | -*) die "недопустимое имя конечного файла: '$out_name'" 4 ;;
esac


tmpdir=$(mktemp -d) || die "не удалось создать временный каталог" 5

base=$(basename -- "$src")
cp -- "$src" "$tmpdir/$base" || die "не удалось скопировать файл в '$tmpdir'" 6
cd "$tmpdir" || die "не удалось перейти в '$tmpdir'" 6


case "$base" in
    *.tex)
        case "$out_name" in
            *.pdf) : ;;
            *)
                out_name="$out_name.pdf"
                echo "build.sh: к имени добавлено .pdf: $out_name" >&2
                ;;
        esac
        TEXINPUTS="$src_abs_dir:" pdflatex -interaction=nonstopmode \
            -halt-on-error "$base" >build.log 2>&1 \
            || { tail -n 20 build.log >&2
                 die "pdflatex завершился с ошибкой" 7; }
        built="${base%.*}.pdf"
        ;;
    *.c)
        cc -O2 -Wall -I "$src_abs_dir" -o "$out_name" "$base" \
            || die "компиляция C завершилась с ошибкой" 7
        built=$out_name
        ;;
    *.cc | *.cpp | *.cxx | *.C)
        c++ -O2 -Wall -I "$src_abs_dir" -o "$out_name" "$base" \
            || die "компиляция C++ завершилась с ошибкой" 7
        built=$out_name
        ;;
    *)
        die "неизвестный тип файла '$base' (поддерживаются .c, .cpp, .cc, .cxx, .tex)" 8
        ;;
esac

[ -f "$built" ] || die "ожидаемый результат '$built' не был создан" 9

# Результат кладём рядом с исходником
cp -- "$tmpdir/$built" "$src_abs_dir/$out_name" \
    || die "не удалось скопировать результат в '$src_abs_dir/$out_name'" 10

echo "Сборка успешна: $src_abs_dir/$out_name"
exit 0