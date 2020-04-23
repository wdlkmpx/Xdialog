%define name	Xdialog
%define version	2.3.1
%define release	1

Name:		%{name}
Summary:	Xdialog is a X11 drop in replacement for cdialog.
Version:	%{version}
Release:	%{release}
Source:		%{name}-%{version}.tar.bz2
Group:		X11/Administration
URL:		http://xdialog.dyns.net/
Copyright:	GPL
Packager:	Thierry Godefroy <xdialog@free.fr>
Prefix:		/usr
BuildRoot:	/var/tmp/%{name}-buildroot
BuildRequires:	gettext
Provides:	Xdialog

%description
Xdialog is designed to be a drop in replacement for the cdialog program.
It converts any terminal based program into a program with an X-windows
interface. The dialogs are easier to see and use and Xdialog adds even
more functionalities (help button+box, treeview, editbox, file selector,
range box, and much more).

%prep
rm -rf $RPM_BUILD_ROOT
%setup -q

%build
# CFLAGS="%optflags"
# ./configure
%configure
# Fix bugs in many automake versions:
RANLIBBIN=`which ranlib`
sed -e "s:RANLIB = @RANLIB@:RANLIB = $RANLIBBIN:" lib/Makefile > lib/Makefile.patched
mv -f lib/Makefile.patched lib/Makefile
XGETTEXTBIN=`which xgettext`
sed -e "s;XGETTEXT = :;XGETTEXT = $XGETTEXTBIN;" po/Makefile > po/Makefile.patched
mv -f po/Makefile.patched po/Makefile

make

%install
%makeinstall

%{find_lang} %{name}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0755)
%doc samples doc/*.html doc/*.png
%_mandir/man1/Xdialog.1*
%_bindir/*
%{prefix}/share/locale/*/LC_MESSAGES/Xdialog.mo

%changelog
see https://github.com/wdlkmpx/Xdialog/commits/
