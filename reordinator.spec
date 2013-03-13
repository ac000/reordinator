Summary:	Reorder lines in a file
Name:		reordinator
Version:	002
Release:	1%{?dist}
Group:		Development/Tools
License:	GPLv2
Packager:	Andrew Clayton <andrew@digital-domain.net>
Url:		https://github.com/ac000/reordinator
Source0:	reordinator-%{version}.tar
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root

%description
A GUI tool to help re-ordering lines in a file

%prep
%setup -q

%build
make

%install
rm -rf $RPM_BUILD_ROOT
install -Dp -m0755 reordinator $RPM_BUILD_ROOT/usr/bin/reordinator
install -Dp -m0644 reordinator.glade $RPM_BUILD_ROOT/usr/share/reordinator/reordinator.glade
install -Dp -m0644 reordinator.desktop $RPM_BUILD_ROOT/usr/share/applications/reordinator.desktop
install -Dp -m0644 reordinator.svg $RPM_BUILD_ROOT/usr/share/pixmaps/reordinator.png

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/bin/reordinator
/usr/share/reordinator/reordinator.glade
/usr/share/applications/reordinator.desktop
/usr/share/pixmaps/reordinator.png

%changelog
* Wed Mar 13 2013 Andrew Clayton <andrew@digital-domain.net> - 002-1
- Update to new version

* Mon Mar 11 2013 Andrew Clayton <andrew@digital-domain.net> - 001-1
- Initial version
