//
//  LoginService.h
//  App
//
//  Created by Ben Tkacheff on 5/1/13.
//
//
#include "v8tree/Instance.h"
#include "v8tree/Service.h"

namespace RBX
{

	extern const char* const sLoginService;
	class LoginService
    : public DescribedNonCreatable<LoginService, Instance, sLoginService>
    , public Service

	{
	public:
		LoginService();

        rbx::signal<void(std::string)> loginSucceededSignal;
        rbx::signal<void(std::string)> loginFailedSignal;

        rbx::signal<void()> promptLoginSignal;
        rbx::signal<void()> promptLogoutSignal;

        void promptSignup();
        void promptLogin();
        void logout();
	};

}
