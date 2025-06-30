-- enum-declaration --
Enum Kind
  A
  B
  C
End Enum
-- --
-- enum-declaration --
Enum Kind
  A : B : C
End Enum
-- --
-- enum-declaration --
Enum Kind
  A : B :
  C
End Enum
-- --
-- enum-declaration --
Enum Kind
  A : B
  C
End Enum
-- --
-- enum-declaration --
Enum Kind
  A : B :
  C
End Enum
-- --
-- enum-declaration --
Enum Kind
  A = 0: B = 1
  C
End Enum
-- --
-- enum-declaration --
Enum Kind
  A = 0: B = 1 Rem hello
  C
End Enum
-- --
-- public-type-declaration --
Public Type StateData 
    CityCode (1 to 100) As Integer 
    County As String * 30 
End Type  
-- --
-- public-type-declaration --
Public Type EmployeeRecord    ' Create user-defined type. 
    ID As Integer    ' Define elements of data type. 
    Name As String * 20 
    Address As String * 30 
    Phone As Long 
    HireDate As Date 
End Type 
-- --
-- function-declaration --
Function CalculateSquareRoot(NumberArg As Double) As Double 
 If 1 Then ' Evaluate argument. 
 Else 
 End If 

End Function
-- --
-- if-statement --
 If 1 Then ' Evaluate argument. 
 Else 
 End If 
-- --

