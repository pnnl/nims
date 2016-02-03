import error

class SDKError(Exception):
    def __init__(self, return_code):
        self.return_code = return_code;
        self.error_name = error.Error.get_name(return_code);
        self.error_message = error.Error.get_string(return_code);

    def __str__(self):
        return "BlueView.SDKError: " + str(self.return_code) + " : " + self.error_name + " : " + self.error_message
